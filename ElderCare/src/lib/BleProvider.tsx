import { createContext, useContext, useEffect, useRef, useState, ReactNode } from 'react';
import { PermissionsAndroid, Platform } from 'react-native';
import { BleManager, Device, Subscription } from 'react-native-ble-plx';
import { Buffer } from 'buffer';
import AsyncStorage from '@react-native-async-storage/async-storage';

// ---- Must match ESP32 firmware ----
const SERVICE_UUID  = '12345678-1234-1234-1234-123456789abc';
const RX_CHAR_UUID  = 'abcd1234-5678-90ab-cdef-123456789abc';
const TX_CHAR_UUID  = 'dcba4321-8765-09ba-fedc-987654321abc';
const STORAGE_KEY   = 'paired_device_id';

export type BleStatus = 'idle' | 'scanning' | 'connecting' | 'connected' | 'disconnected';

type DataHandler = (data: string) => void;

interface BleContextValue {
  status: BleStatus;
  connectedDevice: Device | null;
  discoveredDevices: Device[];
  lastReceived: string;

  startScan: () => Promise<void>;
  stopScan: () => void;
  pair: (device: Device) => Promise<void>;
  send: (text: string) => Promise<void>;
  forget: () => Promise<void>;

  /** Subscribe to BLE data events. Returns an unsubscribe function. */
  onData: (handler: DataHandler) => () => void;
}

const BleContext = createContext<BleContextValue | null>(null);

// Singleton manager — lives for the app's lifetime
const manager = new BleManager();

async function requestPerms(): Promise<boolean> {
  if (Platform.OS === 'android' && Platform.Version >= 31) {
    const r = await PermissionsAndroid.requestMultiple([
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
      PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
    ]);
    return Object.values(r).every(v => v === 'granted');
  }
  return true;
}

export function BleProvider({ children }: { children: ReactNode }) {
  const [status, setStatus] = useState<BleStatus>('idle');
  const [connectedDevice, setConnectedDevice] = useState<Device | null>(null);
  const [discoveredDevices, setDiscoveredDevices] = useState<Device[]>([]);
  const [lastReceived, setLastReceived] = useState<string>('');

  const subRef = useRef<Subscription | null>(null);
  const deviceMapRef = useRef<Map<string, Device>>(new Map());
  // Set in a ref so handlers can come and go without re-creating the BLE plumbing
  const handlersRef = useRef<Set<DataHandler>>(new Set());

  // Auto-reconnect on app launch
  useEffect(() => {
    (async () => {
      const ok = await requestPerms();
      if (!ok) return;

      const savedId = await AsyncStorage.getItem(STORAGE_KEY);
      if (!savedId) return;

      setStatus('connecting');
      try {
        const d = await manager.connectToDevice(savedId, { timeout: 10000 });
        await afterConnect(d);
      } catch {
        setStatus('disconnected');
      }
    })();

    return () => {
      subRef.current?.remove();
      manager.stopDeviceScan();
      // Don't destroy the manager — it's a singleton
    };
  }, []);

  async function startScan(): Promise<void> {
    const ok = await requestPerms();
    if (!ok) return;

    deviceMapRef.current.clear();
    setDiscoveredDevices([]);
    setStatus('scanning');

    manager.startDeviceScan([SERVICE_UUID], null, (error, device) => {
      if (error || !device) return;
      deviceMapRef.current.set(device.id, device);
      setDiscoveredDevices(Array.from(deviceMapRef.current.values())
        .sort((a, b) => (b.rssi ?? -999) - (a.rssi ?? -999)));
    });

    setTimeout(() => {
      manager.stopDeviceScan();
      setStatus(prev => prev === 'scanning' ? 'idle' : prev);
    }, 10000);
  }

  function stopScan(): void {
    manager.stopDeviceScan();
    setStatus(prev => prev === 'scanning' ? 'idle' : prev);
  }

  async function pair(device: Device): Promise<void> {
    manager.stopDeviceScan();
    setStatus('connecting');
    try {
      const d = await device.connect();
      await AsyncStorage.setItem(STORAGE_KEY, d.id);
      await afterConnect(d);
    } catch (e) {
      console.error('Pair failed:', e);
      setStatus('disconnected');
    }
  }

  async function afterConnect(d: Device): Promise<void> {
    await d.discoverAllServicesAndCharacteristics();
    setConnectedDevice(d);
    setStatus('connected');

    subRef.current = d.monitorCharacteristicForService(
      SERVICE_UUID, TX_CHAR_UUID,
      (err, char) => {
        if (err || !char?.value) return;
        const text = Buffer.from(char.value, 'base64').toString('utf-8');
        setLastReceived(text);
        // Fan out to every registered handler
        handlersRef.current.forEach(h => {
          try { h(text); } catch (e) { console.error('BLE handler error:', e); }
        });
      }
    );

    d.onDisconnected(() => {
      subRef.current?.remove();
      subRef.current = null;
      setConnectedDevice(null);
      setStatus('disconnected');
    });
  }

  async function send(text: string): Promise<void> {
    if (!connectedDevice) throw new Error('Not connected');
    const payload = Buffer.from(text, 'utf-8').toString('base64');
    await connectedDevice.writeCharacteristicWithResponseForService(
      SERVICE_UUID, RX_CHAR_UUID, payload
    );
  }

  async function forget(): Promise<void> {
    subRef.current?.remove();
    subRef.current = null;
    await connectedDevice?.cancelConnection();
    await AsyncStorage.removeItem(STORAGE_KEY);
    setConnectedDevice(null);
    setLastReceived('');
    setStatus('idle');
  }

  function onData(handler: DataHandler): () => void {
    handlersRef.current.add(handler);
    return () => { handlersRef.current.delete(handler); };
  }

  const value: BleContextValue = {
    status, connectedDevice, discoveredDevices, lastReceived,
    startScan, stopScan, pair, send, forget, onData,
  };

  return <BleContext.Provider value={value}>{children}</BleContext.Provider>;
}

export function useBle(): BleContextValue {
  const ctx = useContext(BleContext);
  if (!ctx) throw new Error('useBle must be used inside <BleProvider>');
  return ctx;
}

/** Convenience hook: subscribe to BLE data with one line. */
export function useBleData(handler: DataHandler): void {
  const { onData } = useBle();
  useEffect(() => onData(handler), [handler, onData]);
}
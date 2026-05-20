import { useEffect, useState } from 'react';
import {
  View, Text, Pressable, StyleSheet,
  ActivityIndicator, TextInput, Modal, ScrollView,
} from 'react-native';
import { Device } from 'react-native-ble-plx';
import { useBle } from '@/lib/BleProvider';

type PairingScreenProps = {
  visible: boolean;
  onClose: () => void;
};

export default function PairingScreen({ visible, onClose }: PairingScreenProps) {
  const {
    status,
    connectedDevice,
    discoveredDevices,
    lastReceived,
    startScan,
    stopScan,
    pair,
    send,
    forget,
  } = useBle();
  const [sendText, setSendText] = useState<string>('Hello from phone!');

  useEffect(() => {
    if (visible && status === 'connected' && connectedDevice) {
      onClose();
    }
  }, [visible, status, connectedDevice, onClose]);

  useEffect(() => {
    if (!visible) {
      stopScan();
    }
  }, [visible, stopScan]);

  const list = Array.from(discoveredDevices)
    .sort((a, b) => (b.rssi ?? -999) - (a.rssi ?? -999));

  if (!visible) {
    return null;
  }

  // ---- UI ----
  return (
    <Modal visible transparent animationType="fade" onRequestClose={onClose}>
      <View style={s.overlay}>
        <View style={s.sheet}>
          {status === 'connecting' ? (
            <View style={[s.root, s.center]}>
              <ActivityIndicator size="large" color="#3b82f6" />
              <Text style={s.muted}>Connecting...</Text>
            </View>
          ) : status === 'connected' && connectedDevice ? (
            <ScrollView contentContainerStyle={s.sheetContent} showsVerticalScrollIndicator={false}>
              <View style={s.sheetHeader}>
                <Text style={s.title}>Paired</Text>
                <Pressable onPress={onClose} style={s.closeButton}>
                  <Text style={s.closeButtonText}>Close</Text>
                </Pressable>
              </View>

              <View style={s.card}>
                <Text style={s.deviceName}>{connectedDevice.name ?? 'Unnamed'}</Text>
                <Text style={s.muted}>{connectedDevice.id}</Text>
              </View>

              <Text style={s.section}>Last received from device</Text>
              <View style={s.card}>
                <Text style={s.received}>{lastReceived || '(waiting...)'}</Text>
              </View>

              <Text style={s.section}>Send to device</Text>
              <TextInput
                style={s.input}
                value={sendText}
                onChangeText={setSendText}
                placeholder="Type a message"
                placeholderTextColor="#666"
              />
              <Pressable style={s.btn} onPress={() => send(sendText)}>
                <Text style={s.btnText}>Send</Text>
              </Pressable>

              <Pressable style={[s.btn, s.btnDanger, { marginTop: 20 }]} onPress={forget}>
                <Text style={s.btnText}>Forget device</Text>
              </Pressable>
            </ScrollView>
          ) : (
            <ScrollView contentContainerStyle={s.sheetContent} showsVerticalScrollIndicator={false}>
              <View style={s.sheetHeader}>
                <Text style={s.title}>Pair a device</Text>
                <Pressable onPress={onClose} style={s.closeButton}>
                  <Text style={s.closeButtonText}>Close</Text>
                </Pressable>
              </View>

              <Text style={s.muted}>Make sure your device is powered on and nearby.</Text>

              <Pressable
                style={[s.btn, status === 'scanning' && s.btnDanger, { marginTop: 16 }]}
                onPress={status === 'scanning'
                  ? stopScan
                  : startScan}
              >
                <Text style={s.btnText}>{status === 'scanning' ? 'Stop scanning' : 'Scan for devices'}</Text>
              </Pressable>

              {status === 'scanning' && (
                <View style={[s.row, { marginTop: 12 }]}>
                  <ActivityIndicator color="#3b82f6" />
                  <Text style={[s.muted, { marginLeft: 8 }]}>Looking for nearby devices...</Text>
                </View>
              )}

              <View style={{ marginTop: 16 }}>
                {list.length === 0 ? (
                  status === 'scanning' ? null : (
                    <Text style={[s.muted, { textAlign: 'center', marginTop: 40 }]}>No devices found yet</Text>
                  )
                ) : (
                  list.map((item: Device) => (
                    <Pressable key={item.id} style={s.deviceRow} onPress={() => pair(item)}>
                      <View style={{ flex: 1 }}>
                        <Text style={s.deviceName}>{item.name ?? 'Unnamed device'}</Text>
                        <Text style={s.muted}>{item.id}</Text>
                      </View>
                      <Text style={s.rssi}>{item.rssi} dBm</Text>
                    </Pressable>
                  ))
                )}
              </View>
            </ScrollView>
          )}
        </View>
      </View>
    </Modal>
  );
}

const s = StyleSheet.create({
  overlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.45)',
    justifyContent: 'center',
    alignItems: 'center',
    padding: 16,
  },
  sheet: {
    width: '100%',
    maxWidth: 520,
    maxHeight: '90%',
    backgroundColor: '#0e1116',
    borderRadius: 24,
    overflow: 'hidden',
  },
  sheetContent: { padding: 20, backgroundColor: '#0e1116' },
  root: { padding: 20, backgroundColor: '#0e1116' },
  center: { justifyContent: 'center', alignItems: 'center' },
  sheetHeader: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', marginBottom: 8 },
  row: { flexDirection: 'row', alignItems: 'center' },
  title: { color: '#fff', fontSize: 24, fontWeight: '700', marginBottom: 8 },
  section: { color: '#9ca3af', marginTop: 20, marginBottom: 8, fontSize: 13 },
  muted: { color: '#6b7280', fontSize: 12 },
  card: { backgroundColor: '#1a1f2a', padding: 14, borderRadius: 10 },
  deviceName: { color: '#fff', fontSize: 15, fontWeight: '500' },
  rssi: { color: '#9ca3af', fontSize: 12 },
  deviceRow: {
    backgroundColor: '#1a1f2a', padding: 14, borderRadius: 10,
    marginBottom: 8, flexDirection: 'row', alignItems: 'center',
  },
  received: { color: '#34d399', fontSize: 16, fontFamily: 'monospace' },
  input: {
    backgroundColor: '#1f2937', color: '#fff', padding: 12,
    borderRadius: 8, marginBottom: 8,
  },
  btn: { backgroundColor: '#3b82f6', padding: 14, borderRadius: 10, alignItems: 'center' },
  btnDanger: { backgroundColor: '#dc2626' },
  btnText: { color: '#fff', fontWeight: '600' },
  closeButton: { paddingVertical: 8, paddingHorizontal: 10 },
  closeButtonText: { color: '#9ca3af', fontWeight: '600' },
});
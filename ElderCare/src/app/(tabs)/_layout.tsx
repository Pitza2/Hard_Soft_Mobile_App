import { useBle, useBleData } from "@/lib/BleProvider";
import { useAuth } from "@/store/auth";
import { Stack, useRouter } from "expo-router";
import React, { useEffect, useState } from "react";
import { Modal, Pressable, StyleSheet, Text, View } from "react-native";
import {
  SafeAreaView,
  useSafeAreaInsets,
} from "react-native-safe-area-context";
import PairingScreen from "./PairingScreen";

function HeaderMenu({ onOpenPairing }: { onOpenPairing: () => void }) {
  const { logout } = useAuth();
  const router = useRouter();
  const [open, setOpen] = useState(false);
  const insets = useSafeAreaInsets();

  const handleLogout = async () => {
    await logout();
    setOpen(false);
    router.replace("/(auth)/login");
  };

  return (
    <>
      <Pressable onPress={() => setOpen(true)} style={styles.menuButton}>
        <View style={styles.hamburger} />
        <View style={styles.hamburger} />
        <View style={styles.hamburger} />
      </Pressable>

      <Modal visible={open} transparent animationType="fade">
        <Pressable style={styles.modalOverlay} onPress={() => setOpen(false)}>
          <View style={[styles.menuContainer, { marginTop: insets.top + 56 }]}>
            <Pressable
              style={styles.menuItem}
              onPress={() => {
                setOpen(false);
                onOpenPairing();
              }}
            >
              <Text style={styles.menuText}>Pair device</Text>
            </Pressable>
            <Pressable style={styles.menuItem} onPress={handleLogout}>
              <Text style={styles.menuText}>Log out</Text>
            </Pressable>
          </View>
        </Pressable>
      </Modal>
    </>
  );
}

export default function TabsLayout() {
  const { status, connectedDevice } = useBle();
  const [pairingOpen, setPairingOpen] = useState(false);
  const [pairingPrompted, setPairingPrompted] = useState(false);
  useBleData((data) => {
    console.log("Received BLE data:", data);
  });
  useEffect(() => {
    const hasDevice = Boolean(connectedDevice);
    const isReady = status !== 'connecting' && status !== 'scanning';

    if (hasDevice) {
      setPairingPrompted(false);
      return;
    }

    if (isReady && !pairingPrompted) {
      setPairingOpen(true);
      setPairingPrompted(true);
    }
  }, [status, connectedDevice, pairingPrompted]);

  return (
    <View style={styles.layout}>
      <SafeAreaView edges={["top"]} style={styles.appHeader}>
        <HeaderMenu
          onOpenPairing={() => {
            setPairingOpen(true);
            setPairingPrompted(true);
          }}
        />
      </SafeAreaView>

      <Stack screenOptions={{ headerShown: false }}>
        <Stack.Screen name="index" />
      </Stack>

      <PairingScreen
        visible={pairingOpen}
        onClose={() => setPairingOpen(false)}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  layout: {
    flex: 1,
  },
  appHeader: {
    height: 56,
    justifyContent: "center",
    paddingHorizontal: 12,
    backgroundColor: "#fff",
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: "#ddd",
  },
  menuButton: {
    width: 40,
    height: 40,
    justifyContent: "center",
    paddingHorizontal: 6,
  },
  hamburger: {
    height: 2,
    backgroundColor: "#111",
    marginVertical: 2,
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: "rgba(0,0,0,0.3)",
    justifyContent: "flex-start",
  },
  menuContainer: {
    marginLeft: 16,
    backgroundColor: "white",
    borderRadius: 8,
    paddingVertical: 8,
    minWidth: 140,
    elevation: 8,
  },
  menuItem: {
    paddingVertical: 12,
    paddingHorizontal: 16,
  },
  menuText: {
    fontSize: 16,
  },
});

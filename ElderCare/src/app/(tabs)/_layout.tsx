import { useAuth } from "@/store/auth";
import { Stack, useRouter } from "expo-router";
import React, { useState } from "react";
import { Modal, Pressable, StyleSheet, Text, View } from "react-native";
import {
  SafeAreaView,
  useSafeAreaInsets,
} from "react-native-safe-area-context";

function HeaderMenu() {
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
  return (
    <View style={styles.layout}>
      <SafeAreaView edges={["top"]} style={styles.appHeader}>
        <HeaderMenu />
      </SafeAreaView>

      <Stack screenOptions={{ headerShown: false }}>
        <Stack.Screen name="index" />
      </Stack>
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

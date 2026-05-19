// src/app/_layout.tsx
import { AuthProvider, useAuth } from "@/store/auth";
import {
  collection,
  doc,
  getFirestore,
  setDoc,
} from "@react-native-firebase/firestore";
import {
  getMessaging,
  onMessage,
  onTokenRefresh,
} from "@react-native-firebase/messaging";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { Stack } from "expo-router";
import { useEffect } from "react";
import { Alert } from "react-native";

const queryClient = new QueryClient();

function PushNotificationSetup() {
  const { user } = useAuth();

  useEffect(() => {
    const messaging = getMessaging();

    const unsubRefresh = onTokenRefresh(messaging, async (newToken) => {
      if (!user?.uid) return;

      try {
        const db = getFirestore();
        const userRef = doc(collection(db, "users"), user.uid);
        await setDoc(userRef, { fcmToken: newToken }, { merge: true });
      } catch (error) {
        console.warn("Token refresh save failed", error);
      }
    });

    const unsubMessage = onMessage(messaging, (remoteMessage) => {
      const title = remoteMessage.notification?.title ?? "New notification";
      const body = remoteMessage.notification?.body ?? "";
      Alert.alert(title, body);
    });

    return () => {
      unsubRefresh();
      unsubMessage();
    };
  }, [user?.uid]);

  return null;
}

export default function RootLayout() {
  return (
    <QueryClientProvider client={queryClient}>
      <AuthProvider>
        <PushNotificationSetup />
        <Stack screenOptions={{ headerShown: false }} />
      </AuthProvider>
    </QueryClientProvider>
  );
}

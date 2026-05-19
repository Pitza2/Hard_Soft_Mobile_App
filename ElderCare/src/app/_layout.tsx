// src/app/_layout.tsx
import { AuthProvider, useAuth } from "@/store/auth";
import firestore from "@react-native-firebase/firestore";
import messaging from "@react-native-firebase/messaging";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { Stack } from "expo-router";
import { useEffect } from "react";
import { Alert } from "react-native";

const queryClient = new QueryClient();

function PushNotificationSetup() {
  const { user } = useAuth();

  useEffect(() => {
    const unsubRefresh = messaging().onTokenRefresh(async (newToken) => {
      if (!user?.uid) return;

      try {
        await firestore()
          .collection("users")
          .doc(user.uid)
          .set({ fcmToken: newToken }, { merge: true });
      } catch (error) {
        console.warn("Token refresh save failed", error);
      }
    });

    const unsubMessage = messaging().onMessage((remoteMessage) => {
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

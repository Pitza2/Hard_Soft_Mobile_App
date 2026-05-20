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
import * as Notifications from "expo-notifications";
import * as SplashScreen from "expo-splash-screen";
import { useEffect } from "react";
import { ALERTS_CHANNEL_ID } from "@/lib/notifications";
import { BleProvider } from "@/lib/BleProvider";

SplashScreen.preventAutoHideAsync();

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

    const unsubMessage = onMessage(messaging, async (remoteMessage) => {
      await Notifications.scheduleNotificationAsync({
        content: {
          title: remoteMessage.notification?.title ?? "New notification",
          body: remoteMessage.notification?.body ?? "",
          data: remoteMessage.data ?? {},
          sound: "default",
        },
        trigger: {
          channelId: ALERTS_CHANNEL_ID,
        } as Notifications.NotificationTriggerInput,
      });
    });

    return () => {
      unsubRefresh();
      unsubMessage();
    };
  }, [user?.uid]);

  return null;
}

function SplashScreenController() {
  const { loading } = useAuth();

  useEffect(() => {
    if (!loading) {
      SplashScreen.hideAsync();
    }
  }, [loading]);

  return null;
}

export default function RootLayout() {
  return (
    <QueryClientProvider client={queryClient}>
      <AuthProvider>
        <SplashScreenController />
        <PushNotificationSetup />
        <BleProvider>
          <Stack screenOptions={{ headerShown: false }} />
        </BleProvider>
      </AuthProvider>
    </QueryClientProvider>
  );
}
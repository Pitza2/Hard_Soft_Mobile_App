import {
  collection,
  doc,
  getFirestore,
  setDoc,
} from "@react-native-firebase/firestore";
import { getMessaging, getToken } from "@react-native-firebase/messaging";
import * as Device from "expo-device";
import {
  requestNotificationPermission,
  setupAndroidChannel,
} from "@/lib/notifications";

export default async function registerForPushNotifications(uid: string) {
  if (!Device.isDevice) return;

  await setupAndroidChannel();

  const granted = await requestNotificationPermission();
  if (!granted) return;

  const messaging = getMessaging();
  const fcmToken = await getToken(messaging);

  try {
    const db = getFirestore();
    const userRef = doc(collection(db, "users"), uid);
    await setDoc(userRef, { fcmToken }, { merge: true });
  } catch (error) {
    console.warn("Unable to save push token to Firestore", error);
  }
}
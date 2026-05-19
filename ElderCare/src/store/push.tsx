import {
  collection,
  doc,
  getFirestore,
  setDoc,
} from "@react-native-firebase/firestore";
import {
  AuthorizationStatus,
  getMessaging,
  getToken,
  requestPermission,
} from "@react-native-firebase/messaging";
import * as Device from "expo-device";

export default async function registerForPushNotifications(uid: string) {
  if (!Device.isDevice) return;
  const messaging = getMessaging();
  const authStatus = await requestPermission(messaging);
  const enabled =
    authStatus === AuthorizationStatus.AUTHORIZED ||
    authStatus === AuthorizationStatus.PROVISIONAL;

  if (!enabled) return;

  const fcmToken = await getToken(messaging);
  try {
    const db = getFirestore();
    const userRef = doc(collection(db, "users"), uid);
    await setDoc(userRef, { fcmToken }, { merge: true });
  } catch (error) {
    console.warn("Unable to save push token to Firestore", error);
  }
}

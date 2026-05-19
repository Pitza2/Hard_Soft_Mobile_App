import firestore from "@react-native-firebase/firestore";
import messaging from "@react-native-firebase/messaging";
import * as Device from "expo-device";

export default async function registerForPushNotifications(uid: string) {
  if (!Device.isDevice) return;
  console.log("Registering for push notifications with UID:", uid);
  const authStatus = await messaging().requestPermission();
  const enabled =
    authStatus === messaging.AuthorizationStatus.AUTHORIZED ||
    authStatus === messaging.AuthorizationStatus.PROVISIONAL;

  if (!enabled) return;

  const fcmToken = await messaging().getToken();
  console.log("Obtained FCM token:", fcmToken);
  try {
    await firestore()
      .collection("users")
      .doc(uid)
      .set({ fcmToken }, { merge: true });
  } catch (error) {
    console.warn("Unable to save push token to Firestore", error);
  }
}

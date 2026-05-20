import * as Notifications from "expo-notifications";
import { AndroidImportance } from "expo-notifications";

export const ALERTS_CHANNEL_ID = "alerts";

Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowBanner: true,
    shouldShowList: true,
    shouldPlaySound: true,
    shouldSetBadge: false,
  }),
});

export async function setupAndroidChannel() {
  await Notifications.setNotificationChannelAsync(ALERTS_CHANNEL_ID, {
    name: "Medical Alerts",
    importance: AndroidImportance.MAX,
    vibrationPattern: [0, 250, 250, 250],
    enableVibrate: true,
  });
}

export async function requestNotificationPermission() {
  const { status } = await Notifications.getPermissionsAsync();
  if (status === "granted") return true;
  const { status: newStatus } = await Notifications.requestPermissionsAsync();
  return newStatus === "granted";
}
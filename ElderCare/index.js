import {
    getMessaging,
    setBackgroundMessageHandler,
} from "@react-native-firebase/messaging";
import "expo-router/entry";

const messaging = getMessaging();

setBackgroundMessageHandler(messaging, async (remoteMessage) => {});

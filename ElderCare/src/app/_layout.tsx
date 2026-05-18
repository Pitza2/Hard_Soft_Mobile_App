import { Stack } from "expo-router";
import * as SplashScreen from "expo-splash-screen";

SplashScreen.preventAutoHideAsync();

export default function RootLayout() {
  // const [fontsLoaded] = useFonts({
  //   'Inter-Regular': require('../assets/fonts/Inter-Regular.ttf'),
  // });

  // useEffect(() => {
  //   if (fontsLoaded) SplashScreen.hideAsync();
  // }, [fontsLoaded]);

  // if (!fontsLoaded) return null;

  return (
    <Stack>
      <Stack.Screen name="(tabs)" options={{ headerShown: false }} />
      <Stack.Screen name="(auth)/login" options={{ title: "Sign In" }} />
    </Stack>
  );
}

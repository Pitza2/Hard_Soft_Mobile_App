import { Stack } from "expo-router";
import { useAuthStore } from "../../store/auth";

export default function RootLayout() {
  const isLoggedIn = useAuthStore().isLoggedIn;

  return (
    <Stack>
      <Stack.Screen
        name="(tabs)"
        options={{ headerShown: false }}
        redirect={!isLoggedIn} /* redirects to login if not authed */
      />
      <Stack.Screen name="(auth)/login" />
    </Stack>
  );
}

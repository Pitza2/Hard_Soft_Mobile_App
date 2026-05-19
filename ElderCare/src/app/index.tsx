import { useAuth } from "@/store/auth";
import { Redirect } from "expo-router";
export default function IndexRoute() {
  const { user, loading } = useAuth();

  if (loading) return null;

  if (!user) {
    return <Redirect href="/(auth)/login" />;
  }

  return <Redirect href="/(tabs)" />;
}

import { StyleSheet } from "react-native";
import HomeScreen from "./(tabs)";

export default function Index() {
  return <HomeScreen />;
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
  },
});

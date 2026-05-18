import { useRouter } from "expo-router";
import { ScrollView, Text, TouchableOpacity, View } from "react-native";
import { colors } from "../../constants/Theme";
import { useAuth } from "../../store/auth";
import { styles } from "./styles";

export const options = {
  headerShown: false,
};

export default function HomeScreen() {
  const router = useRouter();
  const logout = useAuth().logout;

  const handleLogout = () => {
    logout();
    router.replace("/(auth)/login");
  };

  return (
    <ScrollView
      style={styles.screenWrapper}
      contentContainerStyle={styles.screenContent}
      showsVerticalScrollIndicator={false}
    >
      <View style={styles.headerCard}>
        <View style={styles.headerCopy}>
          <Text style={styles.kicker}>ElderCare</Text>
          <Text style={styles.title}>Geriatric patient dashboard</Text>
          <Text style={styles.subtitle}>
            Daily vitals and care progress for a quick clinical review.
          </Text>
        </View>

        <View style={styles.statusChip}>
          <View style={styles.statusDot} />
          <Text style={styles.statusText}>Stable today</Text>
        </View>
      </View>

      <View style={styles.metricsGrid}>
        <View style={styles.metricCard}>
          <Text style={[styles.metricLabel, { color: colors.red500 }]}>
            Temperature
          </Text>
          <Text style={styles.metricValue}>37.1°C</Text>
          <Text style={styles.metricDetail}>Within normal range</Text>
        </View>

        <View style={styles.metricCard}>
          <Text style={[styles.metricLabel, { color: colors.blue500 }]}>
            Pulse
          </Text>
          <Text style={styles.metricValue}>76 bpm</Text>
          <Text style={styles.metricDetail}>Regular rhythm</Text>
        </View>

        <View style={styles.metricCard}>
          <Text style={[styles.metricLabel, { color: colors.green500 }]}>
            Oxygen saturation
          </Text>
          <Text style={styles.metricValue}>98%</Text>
          <Text style={styles.metricDetail}>SpO2 is stable</Text>
        </View>

        <View style={styles.metricCard}>
          <Text style={[styles.metricLabel, { color: colors.amber500 }]}>
            Steps today
          </Text>
          <Text style={styles.metricValue}>2,480</Text>
          <Text style={styles.metricDetail}>62% of goal completed</Text>
        </View>
      </View>

      <View style={styles.progressCard}>
        <Text style={styles.sectionTitle}>Today's care progress</Text>
        <View style={styles.progressTrack}>
          <View style={styles.progressFill} />
        </View>
        <View style={styles.progressRow}>
          <Text style={styles.progressText}>Medication check-in done</Text>
          <Text style={styles.progressValue}>Completed</Text>
        </View>
        <View style={styles.progressRow}>
          <Text style={styles.progressText}>Hydration reminder</Text>
          <Text style={styles.progressValue}>Done</Text>
        </View>
        <View style={styles.progressRow}>
          <Text style={styles.progressText}>Walking session</Text>
          <Text style={styles.progressValue}>In progress</Text>
        </View>
      </View>

      <View style={styles.footerCard}>
        <View>
          <Text style={styles.footerTitle}>Caregiver access</Text>
          <Text style={styles.footerSubtitle}>
            Review patient status or log out when the shift ends.
          </Text>
        </View>

        <TouchableOpacity onPress={handleLogout} style={styles.logoutButton}>
          <Text style={styles.logoutText}>Log out</Text>
        </TouchableOpacity>
      </View>
    </ScrollView>
  );
}

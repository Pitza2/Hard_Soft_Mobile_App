import { useRouter } from "expo-router";
import { useState } from "react";
import {
  KeyboardAvoidingView,
  Platform,
  Pressable,
  StyleSheet,
  Text,
  TextInput,
  View,
} from "react-native";
import { colors, font, radius, spacing } from "../../constants/Theme";
import { useAuth } from "../../store/auth";

export default function Login() {
  const router = useRouter();
  const login = useAuth().login;
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");

  const handleSignIn = () => {
    const success = login(email, password);

    if (!success) {
      setError("Enter both email and password to continue.");
      return;
    }

    setError("");
    router.replace("/");
  };

  return (
    <KeyboardAvoidingView
      style={styles.screen}
      behavior={Platform.OS === "ios" ? "padding" : undefined}
    >
      <View style={styles.backgroundGlow} />
      <View style={styles.card}>
        <View style={styles.headerBlock}>
          <Text style={styles.kicker}>ElderCare</Text>
          <Text style={styles.title}>Sign in to continue</Text>
          <Text style={styles.subtitle}>
            Use your email and password to access the care dashboard.
          </Text>
        </View>

        <View style={styles.form}>
          <View style={styles.fieldGroup}>
            <Text style={styles.label}>Email</Text>
            <TextInput
              value={email}
              onChangeText={setEmail}
              autoCapitalize="none"
              autoCorrect={false}
              keyboardType="email-address"
              placeholder="caregiver@eldercare.com"
              placeholderTextColor={colors.gray400}
              textContentType="emailAddress"
              style={styles.input}
            />
          </View>

          <View style={styles.fieldGroup}>
            <Text style={styles.label}>Password</Text>
            <TextInput
              value={password}
              onChangeText={setPassword}
              autoCapitalize="none"
              autoCorrect={false}
              placeholder="Enter your password"
              placeholderTextColor={colors.gray400}
              secureTextEntry
              textContentType="password"
              style={styles.input}
            />
          </View>

          {error ? <Text style={styles.error}>{error}</Text> : null}

          <Pressable
            onPress={handleSignIn}
            style={({ pressed }) => [
              styles.button,
              pressed && styles.buttonPressed,
            ]}
          >
            <Text style={styles.buttonText}>Sign In</Text>
          </Pressable>

          <Text style={styles.helperText}>
            This demo accepts any non-empty email and password.
          </Text>

          <View style={styles.registerRow}>
            <Text style={styles.registerText}>Don't have an account?</Text>
            <Pressable onPress={() => router.push("/register")}>
              <Text style={styles.registerLink}> Register</Text>
            </Pressable>
          </View>
        </View>
      </View>
    </KeyboardAvoidingView>
  );
}

const styles = StyleSheet.create({
  screen: {
    flex: 1,
    backgroundColor: colors.background,
    justifyContent: "center",
    padding: spacing.xl,
  },
  backgroundGlow: {
    position: "absolute",
    top: -120,
    right: -60,
    width: 220,
    height: 220,
    borderRadius: 220,
    backgroundColor: colors.blue50,
    opacity: 0.9,
  },
  card: {
    backgroundColor: colors.surface,
    borderRadius: radius.xl,
    padding: spacing.xl,
    borderWidth: 1,
    borderColor: colors.borderLight,
    boxShadow: "0px 3px 8px rgba(30, 50, 100, 0.08)",
    elevation: 5,
  },
  headerBlock: {
    marginBottom: spacing.xl,
  },
  kicker: {
    color: colors.blue500,
    fontSize: font.size.sm,
    fontWeight: font.weight.semibold,
    letterSpacing: 0.8,
    textTransform: "uppercase",
    marginBottom: spacing.sm,
  },
  title: {
    color: colors.gray900,
    fontSize: font.size.xxl,
    fontWeight: font.weight.bold,
    marginBottom: spacing.sm,
  },
  subtitle: {
    color: colors.gray500,
    fontSize: font.size.md,
    lineHeight: 22,
  },
  form: {
    gap: spacing.lg,
  },
  fieldGroup: {
    gap: spacing.sm,
  },
  label: {
    color: colors.gray700,
    fontSize: font.size.sm,
    fontWeight: font.weight.semibold,
  },
  input: {
    minHeight: 52,
    borderRadius: radius.lg,
    borderWidth: 1,
    borderColor: colors.border,
    backgroundColor: colors.surface,
    color: colors.gray900,
    paddingHorizontal: spacing.md,
    fontSize: font.size.md,
  },
  error: {
    color: colors.red500,
    fontSize: font.size.sm,
    fontWeight: font.weight.medium,
  },
  button: {
    minHeight: 52,
    borderRadius: radius.lg,
    backgroundColor: colors.blue500,
    alignItems: "center",
    justifyContent: "center",
  },
  buttonPressed: {
    backgroundColor: colors.blue600,
  },
  buttonText: {
    color: colors.white,
    fontSize: font.size.md,
    fontWeight: font.weight.semibold,
  },
  helperText: {
    color: colors.gray500,
    fontSize: font.size.sm,
    lineHeight: 20,
    textAlign: "center",
  },
  registerRow: {
    flexDirection: "row",
    justifyContent: "center",
    alignItems: "center",
    marginTop: spacing.sm,
  },
  registerText: {
    color: colors.gray500,
    fontSize: font.size.sm,
  },
  registerLink: {
    color: colors.blue500,
    fontSize: font.size.sm,
    fontWeight: font.weight.semibold,
  },
});

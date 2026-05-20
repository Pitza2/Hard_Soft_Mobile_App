import { useRouter } from "expo-router";
import { useState } from "react";
import {
  KeyboardAvoidingView,
  Platform,
  Pressable,
  Text,
  TextInput,
  View
} from "react-native";
import { colors } from "../../constants/Theme";
import { useAuth } from "../../store/auth";
import { styles } from "../../styles/auth/login.styles";

export default function Login() {
  const router = useRouter();
  const login = useAuth().login;
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");

  const handleSignIn = async () => {
    if (!email || !password) {
      setError("Enter both email and password to continue.");
      return;
    }

    const success = login(email, password);

    try {
      setError("");
      await success;
      router.replace("/");
    } catch (e: any) {
      setError(e?.message ?? "Sign-in failed. Please try again.");
    }
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
            <Pressable onPress={() => router.push("/(auth)/register")}>
              <Text style={styles.registerLink}> Register</Text>
            </Pressable>
          </View>
        </View>
      </View>
    </KeyboardAvoidingView>
  );
}

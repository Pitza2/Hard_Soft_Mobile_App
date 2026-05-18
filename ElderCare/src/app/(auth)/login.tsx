import { useRouter } from "expo-router";
import { useState } from "react";
import {
  KeyboardAvoidingView,
  Platform,
  Pressable,
  Text,
  TextInput,
  View,
} from "react-native";
import { colors } from "../../constants/Theme";
import { useAuthStore } from "../../store/auth";
import { styles } from "./login.styles";

// Email validation regex
const EMAIL_REGEX = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

const isValidEmail = (email: string): boolean => {
  return EMAIL_REGEX.test(email);
};

export default function Login() {
  const router = useRouter();
  const authstore = useAuthStore();
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [emailError, setEmailError] = useState("");

  const handleSignIn = () => {
    // Validate email format
    if (email && !isValidEmail(email)) {
      setEmailError("Please enter a valid email address");
      setError("");
      return;
    }

    const success = authstore.login(email, password);

    if (!success) {
      setError("Enter both email and password to continue.");
      setEmailError("");
      return;
    }

    setError("");
    setEmailError("");
    router.replace("/(tabs)");
  };

  const handleEmailChange = (text: string) => {
    setEmail(text);
    // Real-time validation feedback
    if (text && !isValidEmail(text)) {
      setEmailError("Invalid email format");
    } else {
      setEmailError("");
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
              onChangeText={handleEmailChange}
              autoCapitalize="none"
              autoCorrect={false}
              keyboardType="email-address"
              placeholder="caregiver@eldercare.com"
              placeholderTextColor={colors.gray400}
              textContentType="emailAddress"
              style={[styles.input, emailError && styles.inputError]}
            />
            {emailError ? (
              <Text style={styles.fieldError}>{emailError}</Text>
            ) : null}
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
        </View>
      </View>
    </KeyboardAvoidingView>
  );
}

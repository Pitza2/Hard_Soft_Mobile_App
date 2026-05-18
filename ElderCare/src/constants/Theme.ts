/**
 * constants/theme.ts
 * Geriatric Care — Caretaker Dashboard
 *
 * Design intent:
 *  - Clean clinical whites and cool grays as the base (reduces visual fatigue
 *    for caretakers checking statuses throughout a long shift)
 *  - Blue is the primary action/navigation color (trustworthy, medical-grade feel)
 *  - Red reserved exclusively for alerts and critical status — never decorative
 *  - Amber for warnings that need attention but aren't emergencies
 *  - Green for stable / all-clear patient status
 *  - High contrast throughout — many caretakers glance at the screen quickly
 */

export const colors = {
  // ── Base ────────────────────────────────────────────────────────────────
  white: "#FFFFFF",
  background: "#F4F6F9", // slightly cool off-white — easier on the eyes than pure white
  surface: "#FFFFFF", // cards, modals
  surfaceAlt: "#EDF0F5", // secondary surfaces, input backgrounds
  border: "#D5DBE8",
  borderLight: "#E8ECF4",
  shadow: "rgba(30, 50, 100, 0.08)",

  // ── Blues — primary brand & navigation ──────────────────────────────────
  blue50: "#EBF2FF", // tinted backgrounds, selected states
  blue100: "#C7DBFF",
  blue200: "#93B8FF",
  blue400: "#4A82F0",
  blue500: "#2563EB", // primary actions, active nav, links
  blue600: "#1D52D0", // pressed states
  blue900: "#0F2A6B", // dark text on light blue backgrounds

  // ── Reds — alerts & critical status only ────────────────────────────────
  red50: "#FFF0F0", // alert card backgrounds
  red100: "#FFD6D6",
  red400: "#F05252",
  red500: "#DC2626", // critical alert, fall detected, missed medication
  red600: "#B91C1C", // pressed / darkened state
  red900: "#7F1D1D", // text on red backgrounds

  // ── Amber — warnings & needs attention ──────────────────────────────────
  amber50: "#FFFBEB",
  amber100: "#FEF3C7",
  amber400: "#FBBF24",
  amber500: "#D97706", // overdue check-in, low medication stock
  amber900: "#78350F",

  // ── Green — stable & all-clear ──────────────────────────────────────────
  green50: "#F0FDF4",
  green100: "#DCFCE7",
  green400: "#4ADE80",
  green500: "#16A34A", // patient stable, medication taken, vitals normal
  green900: "#14532D",

  // ── Neutrals — text & UI chrome ─────────────────────────────────────────
  gray50: "#F9FAFB",
  gray100: "#F1F3F7",
  gray200: "#E2E6EF",
  gray400: "#9BA8BF",
  gray500: "#6B7A99", // secondary text, placeholders, icons
  gray700: "#374162", // body text
  gray900: "#111827", // headings, primary text
} as const;

// ── Semantic aliases ─────────────────────────────────────────────────────────
// Use these throughout the app instead of raw color values.
// This makes a future dark-mode pass trivial.

export const semantic = {
  // Text
  textPrimary: colors.gray900,
  textSecondary: colors.gray500,
  textDisabled: colors.gray400,
  textInverse: colors.white,
  textLink: colors.blue500,

  // Backgrounds
  bgApp: colors.background,
  bgCard: colors.surface,
  bgInput: colors.surfaceAlt,
  bgSelected: colors.blue50,

  // Borders
  borderDefault: colors.border,
  borderFocus: colors.blue500,

  // Actions
  actionPrimary: colors.blue500,
  actionPrimaryHover: colors.blue600,
  actionPrimaryText: colors.white,

  // Status — patient health states
  statusStable: colors.green500,
  statusStableBg: colors.green50,
  statusStableBorder: colors.green100,

  statusWarning: colors.amber500,
  statusWarningBg: colors.amber50,
  statusWarningBorder: colors.amber100,

  statusCritical: colors.red500,
  statusCriticalBg: colors.red50,
  statusCriticalBorder: colors.red100,

  statusUnknown: colors.gray400,
  statusUnknownBg: colors.gray100,
} as const;

// ── Status helpers ────────────────────────────────────────────────────────────
// Map a patient status string to the right color trio (bg / border / text).

export type PatientStatus = "stable" | "warning" | "critical" | "unknown";

export const statusColors: Record<
  PatientStatus,
  {
    bg: string;
    border: string;
    text: string;
    dot: string;
  }
> = {
  stable: {
    bg: semantic.statusStableBg,
    border: semantic.statusStableBorder,
    text: colors.green900,
    dot: colors.green500,
  },
  warning: {
    bg: semantic.statusWarningBg,
    border: semantic.statusWarningBorder,
    text: colors.amber900,
    dot: colors.amber500,
  },
  critical: {
    bg: semantic.statusCriticalBg,
    border: semantic.statusCriticalBorder,
    text: colors.red900,
    dot: colors.red500,
  },
  unknown: {
    bg: semantic.statusUnknownBg,
    border: colors.border,
    text: colors.gray500,
    dot: colors.gray400,
  },
};

// ── Typography ────────────────────────────────────────────────────────────────

export const font = {
  // Use a system font stack — no extra bundle weight, renders crisply on all devices
  family: {
    base: "System", // swap for 'Inter' if you add custom fonts
    mono: "Courier New",
  },

  size: {
    xs: 11, // labels, timestamps
    sm: 13, // secondary info, captions
    md: 15, // body text (slightly larger than default for readability)
    lg: 17, // card titles, list items
    xl: 20, // screen headings
    xxl: 26, // dashboard hero numbers (heart rate, SpO2, etc.)
    hero: 36, // large patient vitals display
  },

  weight: {
    regular: "400" as const,
    medium: "500" as const,
    semibold: "600" as const,
    bold: "700" as const,
  },

  lineHeight: {
    tight: 1.2,
    normal: 1.5,
    relaxed: 1.75,
  },
} as const;

// ── Spacing ───────────────────────────────────────────────────────────────────
// 4px base grid. Tap targets are kept generous (min 48px) — caretakers often
// use the app while wearing gloves or in a hurry.

export const spacing = {
  px: 1,
  xs: 4,
  sm: 8,
  md: 12,
  lg: 16,
  xl: 24,
  xxl: 32,
  xxxl: 48,
} as const;

export const tapTarget = 48; // minimum touchable area height/width

// ── Border radius ─────────────────────────────────────────────────────────────

export const radius = {
  sm: 4,
  md: 8,
  lg: 12,
  xl: 16,
  full: 999,
} as const;

// ── Shadows ───────────────────────────────────────────────────────────────────

export const shadow = {
  sm: {
    boxShadow: "0px 1px 3px rgba(30, 50, 100, 0.08)",
    elevation: 2,
  },
  md: {
    boxShadow: "0px 3px 8px rgba(30, 50, 100, 0.08)",
    elevation: 5,
  },
  alert: {
    boxShadow: "0px 2px 6px rgba(240, 82, 82, 0.25)",
    elevation: 6,
  },
} as const;

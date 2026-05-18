import { create } from "zustand";

type AuthState = {
  isLoggedIn: boolean;
  login: (email: string, password: string) => boolean;
  logout: () => void;
};

export const useAuthStore = create<AuthState>((set) => ({
  isLoggedIn: false,
  login: (email, password) => {
    const canLogin = email.trim().length > 0 && password.trim().length > 0;

    if (canLogin) {
      set({ isLoggedIn: true });
    }

    return canLogin;
  },
  logout: () => set({ isLoggedIn: false }),
}));

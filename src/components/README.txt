Add a new component by creating a class that implements `Component`.
Register it in `src/main.cpp` by:
1. Including the new header.
2. Creating an instance.
3. Adding it to the `components[]` array.

Each component returns a JSON payload string and the Firebase client writes it to:
/devices/<MAC_ADDRESS>/<component-name>

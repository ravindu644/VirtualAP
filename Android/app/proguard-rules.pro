# R8 reads these on top of proguard-android-optimize.txt. Compose, libsu and the
# coroutines library ship their own consumer rules, so nothing here keeps them whole;
# the Application class, the activity, the tile service and the receiver are kept by
# the manifest.

# Verbose logging is dead weight in a release build.
-assumenosideeffects class android.util.Log {
    public static *** d(...);
    public static *** v(...);
    public static *** i(...);
}

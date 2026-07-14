#ifdef _WIN32

// Hybrid-GPU laptops (Intel iGPU + Nvidia/AMD dGPU) pick which adapter a process runs on based on the
// system-wide "Power saving"/"High performance" default and per-app overrides in Windows' Graphics
// settings. Left on the default, some Intel iGPU drivers (confirmed: Intel Arc iGPU on an HP ZBook
// Firefly 16 G11, at the iGPU's low-power clock state) corrupt the streamed ImGui vertex/index buffers -
// visible as diagonal tearing/garbage that gets worse with larger documents. The dGPU on the same machine
// is unaffected.
//
// Nvidia's and AMD's drivers both look for these exact symbol names exported from the exe and, if
// present and non-zero, default the process to the discrete GPU regardless of the system-wide preference
// - the same effect as manually selecting "High performance" for this app in Windows Settings, just
// without requiring the user to do that by hand.
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 1;
}

#endif

Import("env")

platform = env.PioPlatform()
framework_dir = platform.get_package_dir("framework-arduinoespressif32")
if framework_dir:
    env.Append(CPPPATH=[env.Dir(framework_dir + "/libraries/FS/src")])

import os

Import("env")


framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
if framework_dir:
    env.Append(
        CPPPATH=[
            os.path.join(framework_dir, "libraries", "Network", "src"),
        ]
    )

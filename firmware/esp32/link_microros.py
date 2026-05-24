Import("env")
import os

# micro_ros_arduino ships precompiled libmicroros.a per architecture.
# PlatformIO adds src/ to the library path but not src/esp32/,
# so we add it explicitly here.
libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
pioenv  = env.subst("$PIOENV")
lib_dir = os.path.join(libdeps, pioenv, "micro_ros_arduino", "src", "esp32")

env.Append(LIBPATH=[lib_dir])
env.Append(LIBS=["microros"])

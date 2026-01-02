Import("env")

def generate_compiledb(source, target, env):
    env.Execute("$PYTHONEXE -m platformio run -t compiledb -e $PIOENV -s")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", generate_compiledb)

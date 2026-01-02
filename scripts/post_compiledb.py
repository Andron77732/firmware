Import("env")

# Генерировать compile_commands.json после каждой сборки
env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.elf",
    env.VerboseAction(" ".join([
        "pio", "run", "-t", "compiledb", "-e", env["PIOENV"], "-s"
    ]), "Generating compile_commands.json")
)

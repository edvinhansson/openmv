project "mksdk"
	kind "ConsoleApp"
	language "C"
	cdialect "C99"

	targetdir "../../bin"
	objdir "obj"

	architecture "x64"
	staticruntime "on"

	files {
		"src/mksdk.c",
		"src/microtar.c",
	}

	includedirs {
		"src",
		"../../core/src"
	}

	links {
		"core"
	}

	defines {
		"IMPORT_SYMBOLS"
	}

	filter "configurations:debug"
		defines { "DEBUG" }
		symbols "on"
		runtime "debug"

	filter "configurations:release"
		defines { "RELEASE" }
		optimize "on"
		runtime "release"
const std = @import("std");

const CdbGenStep = struct {
    step: std.Build.Step,
    b: *std.Build,
    frags_dir: []const u8,
    out_path: []const u8,

    pub fn create(b: *std.Build, frags_dir: []const u8, out_path: []const u8) *CdbGenStep {
        const self = b.allocator.create(CdbGenStep) catch @panic("OOM");
        self.* = .{
            .step = std.Build.Step.init(.{
                .id = .custom,
                .name = "gen-compile-commands",
                .owner = b,
                .makeFn = make,
            }),
            .b = b,
            .frags_dir = frags_dir,
            .out_path = out_path,
        };
        return self;
    }

    fn make(step: *std.Build.Step, _: std.Build.Step.MakeOptions) anyerror!void {
        const self: *CdbGenStep = @fieldParentPtr("step", step);

        var cwd = std.fs.cwd();

        // Open fragments directory (created by zig via -gen-cdb-fragment-path).
        var dir = cwd.openDir(self.frags_dir, .{ .iterate = true }) catch {
            return;
        };
        defer dir.close();

        // Write compile_commands.json as a JSON array of fragment objects.
        var out_file = try cwd.createFile(self.out_path, .{ .truncate = true });
        defer out_file.close();
        var write_buffer: [4096]u8 = undefined;

        var ww = out_file.writer(&write_buffer);
        const w = &ww.interface;

        try w.writeByte('[');

        var it = dir.iterate();
        var first: bool = true;
        while (try it.next()) |ent| {
            if (ent.kind != .file) continue;

            // zig emits one JSON object per file
            var frag_file = try dir.openFile(ent.name, .{});
            defer frag_file.close();

            const frag = try frag_file.readToEndAlloc(self.b.allocator, 1024 * 1024);
            defer self.b.allocator.free(frag);

            // skip empty/whitespace-only
            const trimmed = std.mem.trim(u8, frag, " \t\r\n");
            if (trimmed.len == 0) continue;

            if (!first) try w.writeByte(',');
            first = false;
            try w.writeAll(trimmed);
        }

        try w.writeAll("]\n");
        try w.flush();

        cwd.deleteTree(self.frags_dir) catch {};
    }
};

const VERSION = "4.6";

const main_targets: []const std.Target.Query = &[_]std.Target.Query{
    .{ .cpu_arch = .aarch64, .os_tag = .macos, .abi = null },
    .{ .cpu_arch = .x86_64, .os_tag = .macos, .abi = null },
    .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .musl },
    .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .musl },
    .{ .cpu_arch = .x86_64, .os_tag = .windows, .abi = .gnu },
};

const supported_targets: []const std.Target.Query = &[_]std.Target.Query{
    .{ .cpu_arch = .aarch64, .os_tag = .macos, .abi = null },
    .{ .cpu_arch = .x86_64, .os_tag = .macos, .abi = null },
    .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .musl },
    .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .musl },
    .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .gnu },
    .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .gnu },
    .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .gnu, .glibc_version = std.SemanticVersion{ .major = 2, .minor = 27, .patch = 0 } },
    .{ .cpu_arch = .x86_64, .os_tag = .windows, .abi = .gnu },
};

const targets: []const std.Target.Query = main_targets;

const BuildCfg = struct {
    version: []const u8,
    pace: []const u8,
    binary_name: []const u8,
    flags: []const []const u8 = &.{},
    include_test_steps: bool = true,
    cpu_dbg: bool = false,
    mem_dbg: bool = false,
    c3dbg: bool = false,
    snapshot_validation: bool = false,
    urth_mass: bool = false,
    ubsan: bool = false,
    asan: bool = false,
    tracy_enable: bool = false,
    tracy_callstack: bool = false,
    tracy_no_exit: bool = false,
    gen_cdb: bool = false,
};

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{ .whitelist = supported_targets });
    var optimize = b.standardOptimizeOption(.{});

    //
    // Additional Project-Specific Options
    //

    const all = b.option(bool, "all", "Build all targets") orelse false;

    const release = b.option(bool, "release", "Build for release") orelse false;
    if (release) optimize = .ReleaseFast;

    addMesaSessionWasmStep(b, optimize);
    addNounLoomWasmStep(b, optimize);
    addNounHostfsWasmStep(b, optimize);
    addNounEventsWasmStep(b, optimize);
    addNounManageWasmStep(b, optimize);
    addNounBootLiteWasmStep(b, optimize);
    addNounIvoryBootWasmStep(b, optimize);

    const Pace = enum { once, live, soon, edge };
    const pace = @tagName(b.option(
        Pace,
        "pace",
        "Release train (Default: once)",
    ) orelse if (release) Pace.live else Pace.once);

    const copts: []const []const u8 = b.option(
        []const []const u8,
        "copt",
        "Provide additional compiler flags",
    ) orelse &.{};

    const cpu_dbg = b.option(
        bool,
        "cpu-dbg",
        "Enable cpu debug mode (-DU3_CPU_DEBUG)",
    ) orelse false;

    const mem_dbg = b.option(
        bool,
        "mem-dbg",
        "Enable memory debug mode (-DU3_MEMORY_DEBUG)",
    ) orelse false;

    const c3dbg = b.option(
        bool,
        "c3dbg",
        "Enable debug assertions (-DC3DBG)",
    ) orelse false;

    const snapshot_validation = b.option(
        bool,
        "snapshot-validation",
        "Enable snapshot validation (-DU3_SNAPSHOT_VALIDATION)",
    ) orelse false;

    const urth_mass = b.option(
        bool,
        "urth-mass",
        "Enable |mass in urth process (-DU3_URTH_MASS)",
    ) orelse false;

    const binary_name = b.option(
        []const u8,
        "binary-name",
        "Binary name (Default: urbit)",
    ) orelse "urbit";

    const asan = if (target.query.isNative())
        b.option(
            bool,
            "asan",
            "Enable address sanitizer (native only, requires llvm@18)",
        ) orelse false
    else
        false;

    const ubsan = if (target.query.isNative())
        b.option(
            bool,
            "ubsan",
            "Enable undefined behavior sanitizer (native only, requires llvm@18)",
        ) orelse false
    else
        false;

    const tracy_enable = b.option(bool, "tracy", "Enable Tracy profiler") orelse false;
    const tracy_callstack = b.option(bool, "tracy-callstack", "Enable Tracy callstack capture") orelse false;
    const tracy_no_exit = b.option(bool, "tracy-no-exit", "Wait for profiler connection before exiting") orelse false;

    // Parse short git rev
    var file = try std.fs.cwd().openFile(".git/logs/HEAD", .{});
    defer file.close();
    var buf: [4096]u8 = undefined;
    var reader = file.reader(&buf);
    var last_line: [4096]u8 = undefined;
    while (reader.interface.takeDelimiterInclusive('\n')) |line| {
        if (line.len > 0)
            last_line = buf;
    } else |err| if (err != error.EndOfStream) return err;
    const git_rev = buf[41..48];

    // Binary version
    const version = if (!release)
        VERSION ++ "-" ++ git_rev
    else
        VERSION;

    try addMarsBootWasmStep(b, optimize, pace, version);
    try addVereDiskWasmStep(b, optimize, pace, version);

    const gen_cdb = b.option(bool, "generate-commands", "generate compile_commands.json fragments") orelse false;

    //
    // Build
    //

    const build_cfg: BuildCfg = .{
        .version = version,
        .pace = pace,
        .flags = copts,
        .binary_name = binary_name,
        .cpu_dbg = cpu_dbg,
        .mem_dbg = mem_dbg,
        .c3dbg = c3dbg,
        .snapshot_validation = snapshot_validation,
        .urth_mass = urth_mass,
        .asan = asan,
        .ubsan = ubsan,
        .tracy_enable = tracy_enable,
        .tracy_callstack = tracy_callstack,
        .tracy_no_exit = tracy_no_exit,
        .include_test_steps = !all,
        .gen_cdb = gen_cdb,
    };

    if (all) {
        for (targets) |t| {
            try buildBinary(b, b.resolveTargetQuery(t), optimize, build_cfg);
        }
    } else {
        const t = target.result;
        try buildBinary(
            b,
            if (t.os.tag == .linux and
                target.query.isNative() and
                !asan and !ubsan)
                b.resolveTargetQuery(.{ .abi = .musl })
            else
                target,
            optimize,
            build_cfg,
        );
    }
}

fn addNounHostfsWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
) void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
    });

    const step = b.step(
        "noun-hostfs-wasm",
        "Build the noun host filesystem boundary for wasm32-wasi",
    );

    const exe = b.addExecutable(.{
        .name = "noun-hostfs-wasm",
        .root_module = b.createModule(.{
            .target = wasm_target,
            .optimize = optimize,
        }),
    });

    configureBrowserWasmMemory(exe);
    exe.linkLibC();
    exe.addIncludePath(b.path("pkg"));
    exe.addIncludePath(b.path("pkg/noun"));
    exe.addCSourceFiles(.{
        .files = &.{
            "pkg/noun/hostfs_wasm_probe.c",
            "pkg/noun/hostfs.c",
        },
        .flags = &.{
            "-std=gnu23",
            "-Wall",
            "-Werror",
            "-fno-sanitize=all",
            "-DU3_OS_wasm=1",
            "-DU3_OS_ENDIAN_little=1",
        },
    });

    const install = b.addInstallArtifact(exe, .{});
    step.dependOn(&install.step);
}

const wasm_page_size = 64 * 1024;
const wasm32_max_pages = 65536;
const wasm32_max_memory = wasm32_max_pages * wasm_page_size;
const browser_wasm_stack_size = 16 * wasm_page_size;
const browser_wasm_min_memory = 16 * 1024 * 1024;

fn configureBrowserWasmMemory(exe: *std.Build.Step.Compile) void {
    exe.import_memory = true;
    exe.export_memory = true;
    exe.initial_memory = browser_wasm_min_memory;
    exe.max_memory = wasm32_max_memory;
    exe.stack_size = browser_wasm_stack_size;
}

fn addNounEventsWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
) void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
        .cpu_features_add = std.Target.wasm.featureSet(&.{
            .exception_handling,
        }),
    });

    const step = b.step(
        "noun-events-wasm",
        "Compile the noun snapshot/event VM boundary for wasm32-wasi",
    );

    const obj = b.addObject(.{
        .name = "noun-events-wasm",
        .root_module = b.createModule(.{
            .target = wasm_target,
            .optimize = optimize,
        }),
    });

    obj.linkLibC();
    obj.addIncludePath(b.path("pkg"));
    obj.addIncludePath(b.path("pkg/noun"));
    obj.addIncludePath(b.path("pkg/noun/platform/wasm"));
    obj.addIncludePath(b.path("ext/murmur3/vendor"));
    obj.addCSourceFiles(.{
        .files = &.{
            "pkg/noun/events.c",
        },
        .flags = &.{
            "-std=gnu23",
            "-Wall",
            "-Werror",
            "-Wno-unused-function",
            "-mexception-handling",
            "-fno-sanitize=all",
            "-DU3_OS_wasm=1",
            "-DU3_OS_ENDIAN_little=1",
            "-D__wasm_exception_handling__=1",
        },
    });

    step.dependOn(&obj.step);
}

fn addNounManageWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
) void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
        .cpu_features_add = std.Target.wasm.featureSet(&.{
            .exception_handling,
        }),
    });

    const flags: []const []const u8 = &.{
        "-std=gnu23",
        "-Wall",
        "-Werror",
        "-Wno-unused-function",
        "-mexception-handling",
        "-mllvm",
        "-wasm-enable-sjlj",
        "-fno-sanitize=all",
        "-DU3_OS_wasm=1",
        "-DU3_OS_ENDIAN_little=1",
        "-D__wasm_exception_handling__=1",
    };

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });

    const step = b.step(
        "noun-manage-wasm",
        "Compile the noun manager/boot-lite boundary for wasm32-wasi",
    );
    step.dependOn(&pkg_noun.artifact("noun-manage-wasm").step);
}

fn addNounBootLiteWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
) void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
        .cpu_features_add = std.Target.wasm.featureSet(&.{
            .exception_handling,
        }),
    });

    const flags: []const []const u8 = &.{
        "-std=gnu23",
        "-Wall",
        "-Werror",
        "-Wno-unused-function",
        "-Wno-gnu-statement-expression",
        "-mexception-handling",
        "-mllvm",
        "-wasm-enable-sjlj",
        "-fno-sanitize=all",
        "-DU3_OS_wasm=1",
        "-DU3_OS_ENDIAN_little=1",
        "-D__wasm_exception_handling__=1",
    };

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });
    const pkg_ur = b.dependency("pkg_ur", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });
    const gmp = b.dependency("gmp", .{
        .target = wasm_target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "noun-boot-lite-wasm",
        .root_module = b.createModule(.{
            .target = wasm_target,
            .optimize = optimize,
        }),
    });

    configureBrowserWasmMemory(exe);
    exe.linkLibC();
    exe.linkLibrary(pkg_noun.artifact("noun"));
    exe.addIncludePath(pkg_ur.artifact("ur").getEmittedIncludeTree());
    exe.addIncludePath(gmp.artifact("gmp").getEmittedIncludeTree());
    exe.addIncludePath(b.path("pkg"));
    exe.addIncludePath(b.path("pkg/noun"));
    exe.addCSourceFile(.{
        .file = b.path("pkg/noun/boot_lite_wasm_probe.c"),
        .flags = flags,
    });

    const install = b.addInstallArtifact(exe, .{});
    const step = b.step(
        "noun-boot-lite-wasm",
        "Build a linked boot-lite noun runtime probe for wasm32-wasi",
    );
    step.dependOn(&install.step);
}

fn addNounIvoryBootWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
) void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
        .cpu_features_add = std.Target.wasm.featureSet(&.{
            .exception_handling,
        }),
    });

    const flags: []const []const u8 = &.{
        "-std=gnu23",
        "-Wall",
        "-Werror",
        "-Wno-unused-function",
        "-Wno-gnu-statement-expression",
        "-mexception-handling",
        "-mllvm",
        "-wasm-enable-sjlj",
        "-fno-sanitize=all",
        "-DU3_OS_wasm=1",
        "-DU3_OS_ENDIAN_little=1",
        "-D__wasm_exception_handling__=1",
    };

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });
    const pkg_ur = b.dependency("pkg_ur", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });
    const gmp = b.dependency("gmp", .{
        .target = wasm_target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "noun-ivory-boot-wasm",
        .root_module = b.createModule(.{
            .target = wasm_target,
            .optimize = optimize,
        }),
    });

    configureBrowserWasmMemory(exe);
    exe.linkLibC();
    exe.linkLibrary(pkg_noun.artifact("noun"));
    exe.addIncludePath(pkg_ur.artifact("ur").getEmittedIncludeTree());
    exe.addIncludePath(gmp.artifact("gmp").getEmittedIncludeTree());
    exe.addIncludePath(b.path("pkg"));
    exe.addIncludePath(b.path("pkg/noun"));
    exe.addIncludePath(b.path("pkg/vere/ivory"));
    exe.addCSourceFiles(.{
        .files = &.{
            "pkg/noun/ivory_boot_wasm_probe.c",
            "pkg/vere/ivory/ivory.c",
        },
        .flags = flags,
    });

    const install = b.addInstallArtifact(exe, .{});
    const step = b.step(
        "noun-ivory-boot-wasm",
        "Build a linked Ivory-pill noun runtime probe for wasm32-wasi",
    );
    step.dependOn(&install.step);
}

fn addMarsBootWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
    pace: []const u8,
    version: []const u8,
) !void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
        .cpu_features_add = std.Target.wasm.featureSet(&.{
            .exception_handling,
        }),
    });

    var flags_list = std.array_list.Managed([]const u8).init(b.allocator);
    try flags_list.appendSlice(&.{
        "-std=gnu23",
        "-Wall",
        "-Werror",
        "-Wno-unused-function",
        "-Wno-gnu-statement-expression",
        "-mexception-handling",
        "-mllvm",
        "-wasm-enable-sjlj",
        "-fno-sanitize=all",
        "-DU3_OS_wasm=1",
        "-DU3_OS_ENDIAN_little=1",
        "-D__wasm_exception_handling__=1",
    });
    try flags_list.append(b.fmt("-DU3_VERE_PACE=\"{s}\"", .{pace}));
    try flags_list.append(b.fmt("-DURBIT_VERSION=\"{s}\"", .{version}));
    const flags = try flags_list.toOwnedSlice();

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });
    const pkg_ur = b.dependency("pkg_ur", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });
    const gmp = b.dependency("gmp", .{
        .target = wasm_target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "mars-boot-wasm",
        .root_module = b.createModule(.{
            .target = wasm_target,
            .optimize = optimize,
        }),
    });

    configureBrowserWasmMemory(exe);
    exe.linkLibC();
    exe.linkLibrary(pkg_noun.artifact("noun"));
    exe.addIncludePath(pkg_ur.artifact("ur").getEmittedIncludeTree());
    exe.addIncludePath(gmp.artifact("gmp").getEmittedIncludeTree());
    exe.addIncludePath(b.path("pkg"));
    exe.addIncludePath(b.path("pkg/noun"));
    exe.addIncludePath(b.path("pkg/vere"));
    exe.addCSourceFiles(.{
        .files = &.{
            "pkg/vere/mars_boot_wasm_probe.c",
            "pkg/vere/mars_boot.c",
        },
        .flags = flags,
    });

    const install = b.addInstallArtifact(exe, .{});
    const step = b.step(
        "mars-boot-wasm",
        "Build the diskless fake-ship boot constructor for wasm32-wasi",
    );
    step.dependOn(&install.step);
}

fn addVereDiskWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
    pace: []const u8,
    version: []const u8,
) !void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
        .cpu_features_add = std.Target.wasm.featureSet(&.{
            .exception_handling,
        }),
    });

    var flags_list = std.array_list.Managed([]const u8).init(b.allocator);
    try flags_list.appendSlice(&.{
        "-std=gnu23",
        "-Wall",
        "-Werror",
        "-Wno-unused-function",
        "-Wno-gnu-statement-expression",
        "-mexception-handling",
        "-mllvm",
        "-wasm-enable-sjlj",
        "-fno-sanitize=all",
        "-D_WASI_EMULATED_SIGNAL",
        "-DU3_OS_wasm=1",
        "-DU3_OS_ENDIAN_little=1",
        "-D__wasm_exception_handling__=1",
    });
    try flags_list.append(b.fmt("-DU3_VERE_PACE=\"{s}\"", .{pace}));
    try flags_list.append(b.fmt("-DURBIT_VERSION=\"{s}\"", .{version}));
    const flags = try flags_list.toOwnedSlice();

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });
    const pkg_ur = b.dependency("pkg_ur", .{
        .target = wasm_target,
        .optimize = optimize,
        .copt = flags,
    });
    const gmp = b.dependency("gmp", .{
        .target = wasm_target,
        .optimize = optimize,
    });
    const lmdb = b.dependency("lmdb", .{
        .target = wasm_target,
        .optimize = optimize,
    });
    const libuv = b.dependency("libuv", .{
        .target = wasm_target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "vere-disk-wasm",
        .root_module = b.createModule(.{
            .target = wasm_target,
            .optimize = optimize,
        }),
    });

    configureBrowserWasmMemory(exe);
    exe.linkLibC();
    exe.linkLibrary(pkg_noun.artifact("noun"));
    exe.addIncludePath(pkg_ur.artifact("ur").getEmittedIncludeTree());
    exe.addIncludePath(gmp.artifact("gmp").getEmittedIncludeTree());
    exe.addIncludePath(lmdb.artifact("lmdb").getEmittedIncludeTree());
    exe.addIncludePath(libuv.artifact("libuv").getEmittedIncludeTree());
    exe.addIncludePath(b.path("pkg"));
    exe.addIncludePath(b.path("pkg/noun"));
    exe.addIncludePath(b.path("pkg/vere"));
    exe.addCSourceFiles(.{
        .files = &.{
            "pkg/vere/disk_wasm_probe.c",
            "pkg/vere/disk_wasm.c",
            "pkg/vere/mars_boot.c",
            "pkg/vere/ward.c",
        },
        .flags = flags,
    });

    const install = b.addInstallArtifact(exe, .{});
    const step = b.step(
        "vere-disk-wasm",
        "Build the hostfs-backed u3_disk backend probe for wasm32-wasi",
    );
    step.dependOn(&install.step);
}

fn addNounLoomWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
) void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
    });

    const step = b.step(
        "noun-loom-wasm",
        "Build the noun loom allocation boundary for wasm32-wasi",
    );

    const exe = b.addExecutable(.{
        .name = "noun-loom-wasm",
        .root_module = b.createModule(.{
            .target = wasm_target,
            .optimize = optimize,
        }),
    });

    configureBrowserWasmMemory(exe);
    exe.linkLibC();
    exe.addIncludePath(b.path("pkg"));
    exe.addIncludePath(b.path("pkg/noun"));
    exe.addCSourceFiles(.{
        .files = &.{
            "pkg/noun/loom_wasm_probe.c",
            "pkg/noun/loom.c",
            "pkg/noun/log.c",
            "pkg/noun/options.c",
        },
        .flags = &.{
            "-std=gnu23",
            "-Wall",
            "-Werror",
            "-fno-sanitize=all",
            "-DU3_OS_wasm=1",
            "-DU3_OS_ENDIAN_little=1",
        },
    });

    const install = b.addInstallArtifact(exe, .{});
    step.dependOn(&install.step);
}

fn addMesaSessionWasmStep(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
) void {
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .wasi,
        .abi = .musl,
    });

    const step = b.step(
        "mesa-session-wasm",
        "Build the portable Mesa session core for wasm32-wasi",
    );

    const exe = b.addExecutable(.{
        .name = "mesa-session-wasm",
        .root_module = b.createModule(.{
            .target = wasm_target,
            .optimize = optimize,
        }),
    });

    configureBrowserWasmMemory(exe);
    exe.linkLibC();
    exe.addIncludePath(b.path("pkg"));
    exe.addIncludePath(b.path("pkg/noun"));
    exe.addIncludePath(b.path("pkg/vere/io/mesa"));
    exe.addCSourceFiles(.{
        .files = &.{
            "pkg/vere/io/mesa/session_wasm_probe.c",
            "pkg/vere/io/mesa/session.c",
        },
        .flags = &.{
            "-std=gnu23",
            "-Wall",
            "-Werror",
            "-Wno-gnu-statement-expression",
            "-Wno-unused-function",
            "-fno-sanitize=all",
            "-DU3_OS_wasm=1",
            "-DU3_OS_ENDIAN_little=1",
        },
    });

    const install = b.addInstallArtifact(exe, .{});
    step.dependOn(&install.step);
}

fn buildBinary(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cfg: BuildCfg,
) !void {
    const t = target.result;

    //
    // Global C Opts
    // TODO: Propagate these to all 3rd party dependencies
    //

    var global_flags = std.array_list.Managed([]const u8).init(b.allocator);
    defer global_flags.deinit();

    try global_flags.appendSlice(cfg.flags);
    try global_flags.appendSlice(&.{
        "-g3",
        "-Wall",
        "-Werror",
    });

    if (cfg.gen_cdb) {
        try global_flags.appendSlice(&.{
            "-gen-cdb-fragment-path", "cdb",
        });
    }

    if (!cfg.asan and !cfg.ubsan) {
        try global_flags.appendSlice(&.{
            "-fno-sanitize=all",
        });
        if (t.os.tag == .windows) {
            try global_flags.appendSlice(&.{
                "-Qunused-arguments",
            });
        }
    }

    if (cfg.asan and !cfg.ubsan)
        try global_flags.appendSlice(&.{
            "-Wno-deprecated",
            "-fsanitize=address",
            "-fno-sanitize=undefined",
            "-fno-sanitize-trap=undefined",
        });

    if (!cfg.asan and cfg.ubsan)
        try global_flags.appendSlice(&.{
            "-fsanitize=undefined",
            "-fno-sanitize-trap=undefined",
        });

    if (cfg.asan and cfg.ubsan)
        try global_flags.appendSlice(&.{
            "-Wno-deprecated",
            "-fsanitize=address",
            "-fsanitize=undefined",
            "-fno-sanitize-trap=undefined",
        });

    //
    //  C Opts for Urbit PKGs And Binary
    //

    var urbit_flags = std.array_list.Managed([]const u8).init(b.allocator);
    defer urbit_flags.deinit();

    try urbit_flags.appendSlice(global_flags.items);
    try urbit_flags.appendSlice(&.{
        "-Wno-deprecated-non-prototype",
        "-Wno-gnu-binary-literal",
        "-Wno-gnu-empty-initializer",
        "-Wno-gnu-statement-expression",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-gnu",
        "-fno-omit-frame-pointer",
        "-fms-extensions",
        "-DU3_GUARD_PAGE", // pkg_noun
        "-DU3_OS_ENDIAN_little=1", // pkg_c3
        "-DU3_OS_PROF=1", // pkg_c3
    });

    if (cfg.cpu_dbg)
        try urbit_flags.appendSlice(&.{"-DU3_CPU_DEBUG"});

    if (cfg.mem_dbg)
        try urbit_flags.appendSlice(&.{"-DU3_MEMORY_DEBUG"});

    if (cfg.c3dbg)
        try urbit_flags.appendSlice(&.{"-DC3DBG"});

    if (cfg.snapshot_validation)
        try urbit_flags.appendSlice(&.{"-DU3_SNAPSHOT_VALIDATION"});

    if (cfg.urth_mass)
        try urbit_flags.appendSlice(&.{"-DU3_URTH_MASS"});

    if (cfg.tracy_enable) {
        try urbit_flags.appendSlice(&.{"-DTRACY_ENABLE"});
        if (cfg.tracy_callstack) {
            try urbit_flags.appendSlice(&.{"-DTRACY_CALLSTACK"});
        }
        if (cfg.tracy_no_exit) {
            try urbit_flags.appendSlice(&.{"-DTRACY_NO_EXIT"});
        }
    }

    if (t.cpu.arch == .aarch64) {
        try urbit_flags.appendSlice(&.{
            "-DU3_CPU_aarch64=1",
        });
    }

    if (t.os.tag == .macos) {
        try urbit_flags.appendSlice(&.{
            "-DU3_OS_osx=1",
            "-DENT_GETENTROPY_SYSRANDOM", // pkg_ent
        });
    }

    if (t.os.tag == .linux) {
        try urbit_flags.appendSlice(&.{
            "-DU3_OS_linux=1",
            "-DENT_GETENTROPY_UNISTD", //pkg_ent
        });
    }

    if (t.os.tag == .windows) {
        try urbit_flags.appendSlice(&.{
            "-DU3_OS_windows=1",
            "-DWIN32_LEAN_AND_MEAN",
            "-DENT_GETENTROPY_BCRYPTGENRANDOM", // pkg_ent
            "-DO_CLOEXEC=0",
            "-DH2O_NO_UNIX_SOCKETS",
            "-DH2O_NO_HTTP3",
            "-DH2O_NO_REDIS",
            "-DH2O_NO_MEMCACHED",
            "-DCURL_STATICLIB",
        });
    }

    //
    // Dependencies
    //

    const copts: []const []const u8 = urbit_flags.items;

    const pkg_c3 = b.dependency("pkg_c3", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_ent = b.dependency("pkg_ent", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_ur = b.dependency("pkg_ur", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_noun = b.dependency("pkg_noun", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_past = b.dependency("pkg_past", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
    });

    const pkg_vere = b.dependency("pkg_vere", .{
        .target = target,
        .optimize = optimize,
        .copt = copts,
        .pace = cfg.pace,
        .version = cfg.version,
    });

    const curl = b.dependency("curl", .{
        .target = target,
        .optimize = optimize,
    });

    const gmp = b.dependency("gmp", .{
        .target = target,
        .optimize = optimize,
    });

    const h2o = b.dependency("h2o", .{
        .target = target,
        .optimize = optimize,
    });

    const libuv = b.dependency("libuv", .{
        .target = target,
        .optimize = optimize,
    });

    const lmdb = b.dependency("lmdb", .{
        .target = target,
        .optimize = optimize,
    });

    const natpmp = b.dependency("natpmp", .{
        .target = target,
        .optimize = optimize,
    });

    const nghttp3 = b.dependency("nghttp3", .{
        .target = target,
        .optimize = optimize,
    });

    const ngtcp2 = b.dependency("ngtcp2", .{
        .target = target,
        .optimize = optimize,
    });

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const picotls = b.dependency("picotls", .{
        .target = target,
        .optimize = optimize,
    });

    const sigsegv = b.dependency("sigsegv", .{
        .target = target,
        .optimize = optimize,
    });

    const urcrypt = b.dependency("urcrypt", .{
        .target = target,
        .optimize = optimize,
    });

    const whereami = b.dependency("whereami", .{
        .target = target,
        .optimize = optimize,
    });

    const zlib = b.dependency("zlib", .{
        .target = target,
        .optimize = optimize,
    });

    const wasm3 = b.dependency("wasm3", .{
        .target = target,
        .optimize = optimize,
    });

    const tracy = if (cfg.tracy_enable) b.dependency("tracy", .{
        .target = target,
        .optimize = optimize,
    }) else null;

    //
    // Build Artifact
    //

    const urbit = b.addExecutable(.{ .name = cfg.binary_name, .root_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    }) });

    if (t.os.tag == .windows) {
        urbit.stack_size = 67108864;
    } else {
        urbit.stack_size = 0;
    }

    if (t.os.tag == .windows) {
        urbit.linkSystemLibrary("ws2_32"); // WSA*, socket, htons, inet_*, gethostbyname, etc.
    }

    const target_query: std.Target.Query = .{
        .cpu_arch = t.cpu.arch,
        .os_tag = t.os.tag,
        .abi = t.abi,
        .os_version_min = .none,
        .os_version_max = .none,
    };
    const target_output = b.addInstallArtifact(urbit, .{
        .dest_dir = .{
            .override = .{
                .custom = try target_query.zigTriple(b.allocator),
            },
        },
    });
    b.getInstallStep().dependOn(&target_output.step);

    if (target.result.os.tag.isDarwin() and !target.query.isNative()) {
        const macos_sdk = b.lazyDependency("macos_sdk", .{
            .target = target,
            .optimize = optimize,
        });
        if (macos_sdk != null) {
            urbit.addSystemIncludePath(macos_sdk.?.path("usr/include"));
            urbit.addLibraryPath(macos_sdk.?.path("usr/lib"));
            urbit.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
        }
    }

    urbit.linkLibC();

    urbit.linkLibrary(pkg_vere.artifact("vere"));
    urbit.linkLibrary(pkg_noun.artifact("noun"));
    urbit.linkLibrary(pkg_past.artifact("past"));
    urbit.linkLibrary(pkg_c3.artifact("c3"));
    urbit.linkLibrary(pkg_ur.artifact("ur"));

    urbit.linkLibrary(gmp.artifact("gmp"));

    urbit.linkLibrary(h2o.artifact("h2o"));
    urbit.linkLibrary(curl.artifact("curl"));
    urbit.linkLibrary(libuv.artifact("libuv"));
    urbit.linkLibrary(lmdb.artifact("lmdb"));
    urbit.linkLibrary(openssl.artifact("ssl"));
    if (t.os.tag != .windows)
        urbit.linkLibrary(sigsegv.artifact("sigsegv"));
    urbit.linkLibrary(urcrypt.artifact("urcrypt"));
    urbit.linkLibrary(whereami.artifact("whereami"));
    urbit.linkLibrary(wasm3.artifact("wasm3"));

    if (cfg.tracy_enable) {
        urbit.linkLibrary(tracy.?.artifact("tracy"));
        urbit.addIncludePath(tracy.?.path(""));
    }

    if (t.os.tag.isDarwin()) {
        // Requires llvm@18 homebrew installation
        if (cfg.asan or cfg.ubsan)
            urbit.addLibraryPath(.{
                .cwd_relative = "/opt/homebrew/opt/llvm@18/lib/clang/18/lib/darwin",
            });
        if (cfg.asan) urbit.linkSystemLibrary("clang_rt.asan_osx_dynamic");
        if (cfg.ubsan) urbit.linkSystemLibrary("clang_rt.ubsan_osx_dynamic");
    }

    if (t.os.tag == .linux) {
        // Requires llvm-18 and clang-18 installation
        if (cfg.asan or cfg.ubsan)
            urbit.addLibraryPath(.{
                .cwd_relative = "/usr/lib/clang/18/lib/linux",
            });
        if (t.cpu.arch == .x86_64) {
            if (cfg.asan) urbit.linkSystemLibrary("clang_rt.asan-x86_64");
            if (cfg.ubsan)
                urbit.linkSystemLibrary("clang_rt.ubsan_standalone-x86_64");
        }
        if (t.cpu.arch == .aarch64) {
            if (cfg.asan) urbit.linkSystemLibrary("clang_rt.asan-aarch64");
            if (cfg.ubsan)
                urbit.linkSystemLibrary("clang_rt.ubsan_standalone-aarch64");
        }
    }

    urbit.addCSourceFiles(.{
        .root = b.path("pkg/vere"),
        .files = &.{
            "main.c",
        },
        .flags = urbit_flags.items,
    });

    // CI needs generated version.h so we install libvere as a quick fix
    const vere_install = b.addInstallArtifact(pkg_vere.artifact("vere"), .{});
    b.getInstallStep().dependOn(&vere_install.step);

    if (cfg.gen_cdb) {
        const cdb_gen = CdbGenStep.create(b, "cdb", "compile_commands.json");
        const current_install = b.getInstallStep();
        cdb_gen.step.dependOn(current_install);
        b.default_step = &cdb_gen.step;
    }

    //
    // Tests
    //

    if (cfg.include_test_steps) {
        const noun_test_deps = &.{
            pkg_noun.artifact("noun"),
            pkg_c3.artifact("c3"),
            gmp.artifact("gmp"),
        };
        const vere_test_deps = &.{
            pkg_vere.artifact("vere"),
            pkg_noun.artifact("noun"),
            pkg_ur.artifact("ur"),
            pkg_ent.artifact("ent"),
            pkg_c3.artifact("c3"),
            gmp.artifact("gmp"),
            libuv.artifact("libuv"),
            lmdb.artifact("lmdb"),
            natpmp.artifact("natpmp"),
            zlib.artifact("z"),
        };
        const tests = [_]struct {
            name: []const u8,
            file: []const u8,
            deps: []const *std.Build.Step.Compile,
        }{
            // pkg_ur
            .{
                .name = "ur-test",
                .file = "pkg/ur/tests.c",
                .deps = &.{pkg_ur.artifact("ur")},
            },
            // pkg_ent
            .{
                .name = "ent-test",
                .file = "pkg/ent/tests.c",
                .deps = &.{pkg_ent.artifact("ent")},
            },
            // pkg_noun
            .{
                .name = "palloc-test",
                .file = "pkg/noun/palloc_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "equality-test",
                .file = "pkg/noun/equality_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "hashtable-test",
                .file = "pkg/noun/hashtable_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "hostfs-test",
                .file = "pkg/noun/hostfs_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "events-test",
                .file = "pkg/noun/events_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "file-test",
                .file = "pkg/noun/file_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "hamt-test",
                .file = "pkg/vere/hamt_test.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "jets-test",
                .file = "pkg/noun/jets_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "nock-test",
                .file = "pkg/noun/nock_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "bytestream-test",
                .file = "pkg/noun/bytestream_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "retrieve-test",
                .file = "pkg/noun/retrieve_tests.c",
                .deps = noun_test_deps,
            },
            .{
                .name = "serial-test",
                .file = "pkg/noun/serial_tests.c",
                .deps = noun_test_deps,
            },
            // pkg_vere
            .{
                .name = "ames-test",
                .file = "pkg/vere/ames_tests.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "mesa-test",
                .file = "pkg/vere/mesa_tests.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "boot-test",
                .file = "pkg/vere/boot_tests.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "newt-test",
                .file = "pkg/vere/newt_tests.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "vere-noun-test",
                .file = "pkg/vere/noun_tests.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "unix-test",
                .file = "pkg/vere/unix_tests.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "benchmarks",
                .file = "pkg/vere/benchmarks.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "pact-test",
                .file = "pkg/vere/io/mesa/pact_test.c",
                .deps = vere_test_deps,
            },
            .{
                .name = "tracy-test",
                .file = "pkg/vere/tracy_test.c",
                .deps = vere_test_deps,
            },
        };

        for (tests) |tst| {
            const test_step =
                b.step(tst.name, b.fmt("Build & run: {s}", .{tst.file}));
            const test_exe = b.addExecutable(.{ .name = tst.name, .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
            }) });

            if (t.os.tag.isDarwin() and !target.query.isNative()) {
                const macos_sdk = b.lazyDependency("macos_sdk", .{
                    .target = target,
                    .optimize = optimize,
                });
                if (macos_sdk != null) {
                    test_exe.addSystemIncludePath(macos_sdk.?.path("usr/include"));
                    test_exe.addLibraryPath(macos_sdk.?.path("usr/lib"));
                    test_exe.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
                }
            }

            if (t.os.tag == .windows) {
                test_exe.linkSystemLibrary("ws2_32");
            }

            if (t.os.tag.isDarwin()) {
                // Requires llvm@18 homebrew installation
                if (cfg.asan or cfg.ubsan)
                    test_exe.addLibraryPath(.{
                        .cwd_relative = "/opt/homebrew/opt/llvm@18/lib/clang/18/lib/darwin",
                    });
                if (cfg.asan) test_exe.linkSystemLibrary("clang_rt.asan_osx_dynamic");
                if (cfg.ubsan) test_exe.linkSystemLibrary("clang_rt.ubsan_osx_dynamic");
            }

            test_exe.stack_size = 0;
            test_exe.linkLibC();
            for (tst.deps) |dep| {
                test_exe.linkLibrary(dep);
            }
            if (cfg.tracy_enable) {
                test_exe.linkLibrary(tracy.?.artifact("tracy"));
                test_exe.addIncludePath(tracy.?.path(""));
            }
            test_exe.addCSourceFiles(.{
                .files = &.{tst.file},
                .flags = urbit_flags.items,
            });
            const exe_install = b.addInstallArtifact(test_exe, .{});
            const run_unit_tests = b.addRunArtifact(test_exe);
            if (t.os.tag.isDarwin() and (cfg.asan or cfg.ubsan)) {
                //  disable libmalloc warnings
                run_unit_tests.setEnvironmentVariable("MallocNanoZone", "0");
            }
            run_unit_tests.skip_foreign_checks = true;
            test_step.dependOn(&run_unit_tests.step);
            test_step.dependOn(&exe_install.step);
        }

        const quic_loopback_step =
            b.step("quic-loopback", "Build & run ngtcp2/nghttp3/picotls loopback validation");
        const quic_loopback = b.addExecutable(.{ .name = "quic-loopback", .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }) });

        if (t.os.tag.isDarwin() and !target.query.isNative()) {
            const macos_sdk = b.lazyDependency("macos_sdk", .{
                .target = target,
                .optimize = optimize,
            });
            if (macos_sdk != null) {
                quic_loopback.addSystemIncludePath(macos_sdk.?.path("usr/include"));
                quic_loopback.addLibraryPath(macos_sdk.?.path("usr/lib"));
                quic_loopback.addFrameworkPath(macos_sdk.?.path("System/Library/Frameworks"));
            }
        }

        quic_loopback.stack_size = 0;
        quic_loopback.linkLibC();
        quic_loopback.linkLibrary(ngtcp2.artifact("ngtcp2_crypto_picotls"));
        quic_loopback.linkLibrary(ngtcp2.artifact("ngtcp2"));
        quic_loopback.linkLibrary(nghttp3.artifact("nghttp3"));
        quic_loopback.linkLibrary(picotls.artifact("picotls"));
        quic_loopback.linkLibrary(openssl.artifact("ssl"));
        quic_loopback.linkLibrary(openssl.artifact("crypto"));
        quic_loopback.addCSourceFiles(.{
            .files = &.{"pkg/vere/io/mesa/quic_loopback_test.c"},
            .flags = urbit_flags.items,
        });

        const quic_loopback_install = b.addInstallArtifact(quic_loopback, .{});
        const run_quic_loopback = b.addRunArtifact(quic_loopback);
        run_quic_loopback.skip_foreign_checks = true;
        quic_loopback_step.dependOn(&run_quic_loopback.step);
        quic_loopback_step.dependOn(&quic_loopback_install.step);
    }
}

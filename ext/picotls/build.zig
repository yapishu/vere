const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const picotls_c = b.dependency("picotls", .{
        .target = target,
        .optimize = optimize,
    });
    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const picotls = b.addLibrary(.{
        .name = "picotls",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

    picotls.linkLibrary(openssl.artifact("ssl"));
    picotls.linkLibrary(openssl.artifact("crypto"));
    picotls.linkLibC();

    picotls.addIncludePath(picotls_c.path("include"));
    picotls.addIncludePath(picotls_c.path("lib"));

    if (t.os.tag == .linux) {
        picotls.root_module.addCMacro("_GNU_SOURCE", "1");
    }
    picotls.root_module.addCMacro("PICOTLS_USE_DTRACE", "0");

    picotls.addCSourceFiles(.{
        .root = picotls_c.path("."),
        .files = &.{
            "lib/hpke.c",
            "lib/picotls.c",
            "lib/pembase64.c",
            "lib/openssl.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
            "-std=gnu11",
            "-Wall",
            "-O2",
        },
    });

    picotls.installHeadersDirectory(picotls_c.path("include"), "", .{
        .include_extensions = &.{".h"},
    });

    b.installArtifact(picotls);
}

const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const nghttp3_c = b.dependency("nghttp3", .{
        .target = target,
        .optimize = optimize,
    });

    const nghttp3 = b.addLibrary(.{
        .name = "nghttp3",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

    nghttp3.linkLibC();
    nghttp3.root_module.addCMacro("BUILDING_NGHTTP3", "");
    nghttp3.root_module.addCMacro("NGHTTP3_STATICLIB", "");
    if (t.os.tag != .windows) {
        nghttp3.root_module.addCMacro("HAVE_ARPA_INET_H", "1");
        nghttp3.root_module.addCMacro("HAVE_NETINET_IN_H", "1");
        nghttp3.root_module.addCMacro("HAVE_UNISTD_H", "1");
    }

    nghttp3.addIncludePath(nghttp3_c.path("lib/includes"));
    nghttp3.addIncludePath(nghttp3_c.path("lib"));

    nghttp3.addCSourceFiles(.{
        .root = nghttp3_c.path("lib"),
        .files = &nghttp3_sources,
        .flags = &.{
            "-fno-sanitize=all",
            "-std=gnu11",
            "-Wall",
            "-O2",
        },
    });

    nghttp3.installHeadersDirectory(nghttp3_c.path("lib/includes"), "", .{
        .include_extensions = &.{".h"},
    });

    b.installArtifact(nghttp3);
}

const nghttp3_sources = [_][]const u8{
    "nghttp3_balloc.c",
    "nghttp3_buf.c",
    "nghttp3_callbacks.c",
    "nghttp3_conn.c",
    "nghttp3_conv.c",
    "nghttp3_debug.c",
    "nghttp3_err.c",
    "nghttp3_frame.c",
    "nghttp3_gaptr.c",
    "nghttp3_http.c",
    "nghttp3_idtr.c",
    "nghttp3_ksl.c",
    "nghttp3_map.c",
    "nghttp3_mem.c",
    "nghttp3_objalloc.c",
    "nghttp3_opl.c",
    "nghttp3_pq.c",
    "nghttp3_qpack.c",
    "nghttp3_qpack_huffman.c",
    "nghttp3_qpack_huffman_data.c",
    "nghttp3_range.c",
    "nghttp3_ratelim.c",
    "nghttp3_rcbuf.c",
    "nghttp3_ringbuf.c",
    "nghttp3_settings.c",
    "nghttp3_str.c",
    "nghttp3_stream.c",
    "nghttp3_tnode.c",
    "nghttp3_unreachable.c",
    "nghttp3_vec.c",
    "nghttp3_version.c",
    "sfparse/sfparse.c",
};

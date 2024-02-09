const hm = @cImport({
    @cInclude("test-hm.c");
});

const std = @import("std");

test "hash works" {
    const a = hm.hash64to64(3);
    const b = hm.hash64to64(4);
    const c = hm.hash64to64(3);
    try std.testing.expectEqual(a, c);
    try std.testing.expect(b != a);
}

test "create insert-get one" {
    var m = hm.create(@sizeOf(hm.Entry), 4);
    const v: hm.Vtype = hm.Vtype{ .addrs = 4, .tid = 0 };
    const k = 3;

    hm.insert(m, k, v);
    const v2 = hm.get(m, k);

    try std.testing.expectEqual(v2, v);
}

test "create insert-get realloc" {
    var m = hm.create(@sizeOf(hm.Entry), 1);
    const v: hm.Vtype = hm.Vtype{ .addrs = 4, .tid = 0 };
    const k = 3;

    hm.insert(m, k, v);
    hm.insert(m, 4, v);
    const v2 = hm.get(m, k);

    try std.testing.expectEqual(v2, v);
}

test "create delete-get one" {
    var m = hm.create(@sizeOf(hm.Entry), 4);
    const v: hm.Vtype = hm.Vtype{ .addrs = 4, .tid = 0 };
    const k = 1;

    hm.insert(m, k, v);
    hm.erase(m, k);
    const v2 = hm.get(m, k);
    try std.testing.expectEqual(v2, hm.special_null_val());
}

test "get no insert" {
    var m = hm.create(@sizeOf(hm.Entry), 4);
    const k = 3;

    const v2 = hm.get(m, k);

    try std.testing.expectEqual(v2, hm.special_null_val());
}

test "double insert" {
    var m = hm.create(@sizeOf(hm.Entry), 4);
    const v1: hm.Vtype = hm.Vtype{ .addrs = 4, .tid = 0 };
    const v2: hm.Vtype = hm.Vtype{ .addrs = 9, .tid = 9 };
    const k = 9;

    hm.insert(m, k, v1);
    hm.insert(m, k, v2);
    const r = hm.get(m, k);

    try std.testing.expectEqual(v2, r);
}

test "insert delete insert insert-realloc get" {
    var m = hm.create(@sizeOf(hm.Entry), 1);
    const v1: hm.Vtype = hm.Vtype{ .addrs = 4, .tid = 0 };
    const v2: hm.Vtype = hm.Vtype{ .addrs = 3, .tid = 1 };
    const v3: hm.Vtype = hm.Vtype{ .addrs = 2, .tid = 2 };
    const k1 = 3;
    const k2 = 4;
    const k3 = 5;

    hm.insert(m, k1, v1);
    hm.erase(m, k1);
    hm.insert(m, k2, v2);
    hm.insert(m, k3, v3);

    const r1 = hm.get(m, k1);
    const r2 = hm.get(m, k2);
    const r3 = hm.get(m, k3);

    try std.testing.expectEqual(r1, hm.special_null_val());
    try std.testing.expectEqual(r2, v2);
    try std.testing.expectEqual(r3, v3);
}

test "likely probing insert delete others, then get -- no-realloc" {
    var m = hm.create(@sizeOf(hm.Entry), 6);
    const v1: hm.Vtype = hm.Vtype{ .addrs = 4, .tid = 0 };
    const v2: hm.Vtype = hm.Vtype{ .addrs = 3, .tid = 1 };
    const v3: hm.Vtype = hm.Vtype{ .addrs = 2, .tid = 2 };
    const v4: hm.Vtype = hm.Vtype{ .addrs = 9, .tid = 9 };
    const k1 = 31;
    const k2 = 41;
    const k3 = 51;
    const k4 = 61;

    hm.insert(m, k1, v1);
    hm.insert(m, k2, v2);
    hm.insert(m, k3, v3);
    hm.insert(m, k4, v4);
    hm.erase(m, k1);
    hm.erase(m, k2);
    hm.erase(m, k3);

    const r = hm.get(m, k4);
    try std.testing.expectEqual(r, v4);
}

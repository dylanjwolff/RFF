const std = @import("std");

const c = @cImport({
    @cInclude("stdlib.h");
});

const Dist = enum(u16) { Uniform, Normal };

const EventId = usize;

const ReadFrom = extern struct {
    r: EventId,
    w: EventId,
};

const Sched = extern struct {
    random_seed: u16,
    delay: u16,
    dist: Dist,
    avoid_uninit_writes: bool,
    pos_len: usize,
    neg_len: usize,
    obligations: ?[*]ReadFrom,

    fn len(self: Sched) usize {
        return self.pos_len + self.neg_len;
    }

    fn size_bytes(self: Sched) usize {
        return @sizeOf(Sched) + (self.pos_len + self.neg_len) * @sizeOf(ReadFrom);
    }

    fn is_flat(self: *const Sched) bool {
        if (self.obligations == null) {
            return true;
        }
        const a1 = @intFromPtr(self.obligations.?);
        const a2 = @intFromPtr(self);
        return a2 + @sizeOf(Sched) == a1;
    }

    fn as_buf(self: *Sched) []align(8) u8 {
        std.debug.assert(self.is_flat());
        const p = @as([*]align(8) u8, @ptrCast(self))[0..self.size_bytes()];
        return p;
    }

    fn deinit_flat(self: *Sched, allocator: std.mem.Allocator) void {
        std.debug.assert(self.is_flat());
        if (self.neg_len + self.pos_len == 0) {
            return;
        }

        if (self.is_flat()) {
            allocator.free(self.as_buf());
        }
    }

    fn deinit(self: *Sched, allocator: std.mem.Allocator) void {
        if (self.len() > 0) {
            allocator.free(self.obligations.?[0..self.len()]);
        }

        allocator.destroy(self);
    }

    fn pos_obligs(self: Sched) []ReadFrom {
        if (self.pos_len > 0) {
            return self.obligations.?[0..self.pos_len];
        } else {
            return &[_]ReadFrom{};
        }
    }

    fn neg_obligs(self: Sched) []ReadFrom {
        if (self.neg_len > 0) {
            return self.obligations.?[self.pos_len..self.len()];
        } else {
            return &[_]ReadFrom{};
        }
    }

    fn obligs(self: Sched) []ReadFrom {
        if (self.len() > 0) {
            return self.obligations.?[0..self.len()];
        } else {
            return &[_]ReadFrom{};
        }
    }
};

fn serialize(sched: *const Sched, allocator: std.mem.Allocator) []align(8) u8 {
    const buf = allocator
        .alignedAlloc(u8, @alignOf(ReadFrom), sched.size_bytes()) catch @panic("OOM");
    serialize_to(sched.*, buf);
    return buf;
}

fn serialize_to(sched: Sched, chunk: []align(8) u8) void {
    var sched_mem = std.mem.bytesAsSlice(Sched, chunk[0..@sizeOf(Sched)]);
    std.mem.copy(Sched, sched_mem, &[_]Sched{sched});

    var new_sched = @as(*Sched, @ptrCast(chunk));
    var obl = std.mem.bytesAsSlice(ReadFrom, chunk[@sizeOf(Sched)..chunk.len]);
    if (new_sched.pos_len + new_sched.neg_len > 0) {
        new_sched.obligations = obl.ptr;
    } else {
        new_sched.obligations = null;
    }

    var i: usize = 0;
    while (i < sched.pos_len + sched.neg_len) {
        obl[i] = sched.obligations.?[i];
        i += 1;
    }
}

fn deserialize_from(chunk: []align(8) u8) *Sched {
    var new_sched = @as(*Sched, @ptrCast(chunk));
    const sched_len = new_sched.pos_len + new_sched.neg_len;
    if (sched_len > 0) {
        const sched_end = @sizeOf(Sched) + sched_len * @sizeOf(ReadFrom);
        var obl = std.mem.bytesAsSlice(ReadFrom, chunk[@sizeOf(Sched)..sched_end]);
        new_sched.obligations = obl.ptr;
    } else {
        new_sched.obligations = null;
    }

    return new_sched;
}

export fn read_sched(filename: [*c]const u8) *Sched {
    const fname = std.mem.span(filename);
    const allocator = std.heap.c_allocator;
    return inner_read_sched(fname, allocator);
}

fn inner_read_sched(fname: []const u8, allocator: std.mem.Allocator) *Sched {
    const buf = std.fs.cwd().readFileAllocOptions(allocator, fname, 50 * 1000, null, @alignOf(ReadFrom), null) catch @panic("Failed to read file");
    return deserialize_from(buf);
}

export fn write_sched(filename: [*c]const u8, sched: *Sched) void {
    const fname = std.mem.span(filename);
    const buf = sched.as_buf();
    std.fs.cwd().writeFile(fname, buf) catch @panic("Failed to write to file");
}

const SchedJSON = struct {
    random_seed: u16,
    delay: u16,
    dist: u16,
    avoid_uninit_writes: bool,
    pos_len: usize,
    neg_len: usize,
    obligations: ?[]ReadFrom,

    fn to(self: SchedJSON, allocator: std.mem.Allocator) *Sched {
        var sched = allocator.create(Sched) catch @panic("OOM");
        var om: ?[*]ReadFrom = undefined;
        if (self.obligations) |o| {
            var mem = allocator.alloc(ReadFrom, o.len) catch @panic("OOM");
            std.mem.copy(ReadFrom, mem, o);
            om = mem.ptr;
        } else {
            om = null;
        }
        sched.random_seed = self.random_seed;
        sched.delay = self.delay;
        sched.dist = @as(Dist, @enumFromInt(self.dist));
        sched.avoid_uninit_writes = self.avoid_uninit_writes;
        sched.pos_len = self.pos_len;
        sched.neg_len = self.neg_len;
        sched.obligations = om;
        return sched;
    }

    fn from(sched: Sched) SchedJSON {
        var obs = if (sched.obligations) |o| o[0..sched.len()] else null;
        return SchedJSON{
            .random_seed = sched.random_seed,
            .delay = sched.delay,
            .dist = @intFromEnum(sched.dist),
            .avoid_uninit_writes = sched.avoid_uninit_writes,
            .pos_len = sched.pos_len,
            .neg_len = sched.neg_len,
            .obligations = obs,
        };
    }
};

export fn write_json_sched(filename: [*c]const u8, sched: *Sched) void {
    const fname = std.mem.span(filename);
    const sched_json = SchedJSON.from(sched.*);
    var file = std.fs.cwd().createFile(fname, .{}) catch @panic("fopen err");
    defer file.close();

    const options = std.json.StringifyOptions{
        .whitespace = .indent_1,
    };

    std.json.stringify(sched_json, options, file.writer()) catch @panic("failed to serialize JSON");
    _ = file.write("\n") catch @panic("file write err");
}

export fn read_json_sched(filename: [*c]const u8) *Sched {
    const fname = std.mem.span(filename);
    return inner_read_json_sched(fname, std.heap.c_allocator);
}

fn inner_read_json_sched(fname: []const u8, allocator: std.mem.Allocator) *Sched {
    var content = std.fs.cwd().readFileAlloc(allocator, fname, 100 * 1000) catch @panic("file read err");
    defer allocator.free(content);

    const parse_options = std.json.ParseOptions{};

    const parsed = std.json.parseFromSlice(SchedJSON, allocator, content, parse_options) catch @panic("parse err");
    defer parsed.deinit();

    return parsed.value.to(allocator);
}

test "write / read json" {
    const allocator = std.testing.allocator;
    var rf = allocator.alloc(ReadFrom, 1) catch @panic("OOM");
    defer allocator.free(rf);
    rf[0].r = 0;
    rf[0].w = 1;
    var s = Sched{ .avoid_uninit_writes = false, .random_seed = 0, .delay = 1, .dist = Dist.Normal, .pos_len = 1, .neg_len = 0, .obligations = rf.ptr };

    const path: []const u8 = "/tmp/test.schd.json"[0..19];
    write_json_sched(path.ptr, &s);
    var sched = inner_read_json_sched(path, allocator);
    defer sched.deinit(allocator);

    try std.testing.expectEqual(sched.*.avoid_uninit_writes, s.avoid_uninit_writes);
    try std.testing.expectEqual(sched.*.delay, s.delay);
    try std.testing.expectEqual(sched.*.dist, s.dist);
    try std.testing.expectEqual(sched.*.pos_len, s.pos_len);
    try std.testing.expectEqual(sched.*.neg_len, s.neg_len);
    try std.testing.expectEqual(sched.*.obligations.?[0], s.obligations.?[0]);
}

test "write / read json, no rfs" {
    const allocator = std.testing.allocator;
    var s = Sched{ .avoid_uninit_writes = false, .random_seed = 0, .delay = 1, .dist = Dist.Normal, .pos_len = 0, .neg_len = 0, .obligations = null };

    const path: []const u8 = "/tmp/test.schd.json"[0..19];
    write_json_sched(path.ptr, &s);
    var sched = inner_read_json_sched(path, allocator);
    defer sched.deinit(allocator);

    try std.testing.expectEqual(sched.*.obligations, s.obligations);
}

fn inner_write_sched(fname: []const u8, sched: *Sched, allocator: std.mem.Allocator) void {
    const buf = serialize(sched, allocator);
    defer allocator.free(buf);
    std.fs.cwd().writeFile(fname, buf) catch @panic("Failed to write to file");
}

test "smoke" {
    const allocator = std.testing.allocator;
    var rf = allocator.alloc(ReadFrom, 1) catch @panic("OOM");
    defer allocator.free(rf);
    rf[0].r = 0;
    rf[0].w = 1;
    var s = Sched{ .avoid_uninit_writes = false, .random_seed = 0, .delay = 1, .dist = Dist.Normal, .pos_len = 1, .neg_len = 0, .obligations = rf.ptr };

    const buf = serialize(&s, allocator);
    defer allocator.free(buf);

    const path: []const u8 = "/tmp/test.sched.bin"[0..19];
    inner_write_sched(path, &s, allocator);
    var sched = inner_read_sched(path, allocator);
    defer sched.deinit_flat(allocator);

    try std.testing.expectEqual(sched.*.avoid_uninit_writes, s.avoid_uninit_writes);
    try std.testing.expectEqual(sched.*.delay, s.delay);
    try std.testing.expectEqual(sched.*.dist, s.dist);
    try std.testing.expectEqual(sched.*.pos_len, s.pos_len);
    try std.testing.expectEqual(sched.*.neg_len, s.neg_len);
    try std.testing.expectEqual(sched.*.obligations.?[0], s.obligations.?[0]);
}

const SchedEvents = struct {
    rels: []ReadFrom,
    scores: []f32,
};

var READ_ITER: usize = 0;
export fn read_rf_set(filename: [*c]const u8) bool {
    READ_ITER += 1;
    const fname = std.mem.span(filename);
    var allocator = std.heap.c_allocator;

    const r = inner_read_rf_set2(fname, allocator);

    // dump_rf_set("cum-rf-set.json", RF_CUM_SCORES);
    // const f = std.fmt.allocPrint(allocator, "rf-set-{}.json", .{READ_ITER}) catch @panic("OOM");
    // defer allocator.free(f);

    // const inf = std.fmt.allocPrintZ(allocator, "cp {s} inf-{}.json", .{ fname, READ_ITER }) catch @panic("OOM");
    // defer allocator.destroy(inf);
    // _ = c.system(inf);

    // const clog = std.fmt.allocPrintZ(allocator, "cp events.log events-{}.log", .{READ_ITER}) catch @panic("OOM");
    // defer allocator.destroy(clog);
    // _ = c.system(clog);

    // const slog = std.fmt.allocPrintZ(allocator, "cp SCHEDULE SCHEDULE-{}.sched", .{READ_ITER}) catch @panic("OOM");
    // defer allocator.destroy(slog);
    // _ = c.system(slog);

    // dump_rf_set(f, RF_SCORES);
    return r;
}

var CUM_SCORE: f32 = 0;
var L2 = false;
fn inner_read_rf_set2(filename: []const u8, allocator: std.mem.Allocator) bool {
    var content = std.fs.cwd().readFileAlloc(allocator, filename, 10 * 1000 * 1000) catch |e| {
        std.debug.print("FOPEN err on {s}: {}", .{ filename, e });
        return false;
    };
    defer allocator.free(content);

    var has_race = false;

    const parse_options = std.json.ParseOptions{};

    const parsed = std.json.parseFromSlice(SchedEvents, allocator, content, parse_options) catch @panic("parse err");
    defer parsed.deinit();

    var i: u32 = 0;
    while (i < parsed.value.rels.len) {
        // Scores are from 0-1, 0 being racy 1 is not racy. Still want to sample low scores with some small
        // probability
        if (parsed.value.scores[i] == 0) {
            has_race = true;
        }
        var score: f32 = undefined;
        if (L2) {
            score = (1.05 - parsed.value.scores[i]) * (1.05 - parsed.value.scores[i]);
        } else {
            score = (1.05 - parsed.value.scores[i]);
        }

        if (RF_SCORES.get(parsed.value.rels[i])) |old_score| {
            score = @max(old_score, score);
        }
        RF_SCORES.put(parsed.value.rels[i], score) catch @panic("OOM");
        RF_CUM_SCORES.put(parsed.value.rels[i], score) catch @panic("OOM");
        i += 1;
    }

    std.debug.assert(RF_CUM_SCORES.values().len == RF_SCORES.values().len);
    CUM_SCORE = 0;
    var ii: usize = 0;
    const vals = RF_CUM_SCORES.values();
    const keys = RF_CUM_SCORES.keys();
    while (ii < RF_CUM_SCORES.values().len) {
        var score = RF_SCORES.get(keys[ii]).?;
        CUM_SCORE += score;
        vals[ii] = CUM_SCORE;

        ii += 1;
    }
    return has_race;
}

fn inner_read_rf_set(filename: []const u8, allocator: std.mem.Allocator) std.AutoArrayHashMap(ReadFrom, f32) {
    var content = std.fs.cwd().readFileAlloc(allocator, filename, 1 * 1000 * 1000) catch @panic("file read err");
    defer allocator.free(content);

    const parse_options = std.json.ParseOptions{
        .allocator = allocator,
    };

    var ts = std.json.TokenStream.init(content);
    const parsed = std.json.parse(SchedEvents, &ts, parse_options) catch @panic("parse err");
    defer std.json.parseFree(SchedEvents, parsed, parse_options);

    var rf_scores = std.AutoArrayHashMap(ReadFrom, f32).init(allocator);
    var i: u32 = 0;
    while (i < parsed.rels.len) {
        rf_scores.put(parsed.rels[i], parsed.scores[i]) catch @panic("OOM");
        i += 1;
    }
    return rf_scores;
}

fn dump_rf_set(filename: []const u8, rf_set: std.AutoArrayHashMap(ReadFrom, f32)) void {
    const se = SchedEvents{ .rels = rf_set.keys(), .scores = rf_set.values() };

    var file = std.fs.cwd().createFile(filename, .{}) catch @panic("fopen err");
    defer file.close();

    const options = std.json.StringifyOptions{
        .whitespace = .indent_1,
    };

    std.json.stringify(se, options, file.writer()) catch @panic("failed to serialize JSON");
    _ = file.write("\n") catch @panic("file write err");
}

test "smoke event set" {
    const allocator = std.testing.allocator;
    var rf = allocator.create(ReadFrom) catch @panic("OOM");
    defer allocator.destroy(rf);
    rf.r = 0;
    rf.w = 1;

    var rf2 = allocator.create(ReadFrom) catch @panic("OOM");
    defer allocator.destroy(rf2);
    rf2.r = 9;
    rf2.w = 9;

    var rf_scores = std.AutoArrayHashMap(ReadFrom, f32).init(allocator);
    defer rf_scores.deinit();
    rf_scores.put(rf.*, 0.2) catch @panic("OOM");
    rf_scores.put(rf2.*, 0.8) catch @panic("OOM");
    dump_rf_set("data.json", rf_scores);
    defer _ = c.system("rm -f data.json");

    RF_SCORES = std.AutoArrayHashMap(ReadFrom, f32).init(allocator);
    defer RF_SCORES.deinit();
    RF_CUM_SCORES = std.AutoArrayHashMap(ReadFrom, f32).init(allocator);
    defer RF_CUM_SCORES.deinit();
    _ = inner_read_rf_set2("data.json", allocator);

    try std.testing.expect(RF_SCORES.values().len == 2);
    try std.testing.expect(RF_CUM_SCORES.values().len == 2);
    try std.testing.expect(CUM_SCORE <= 1.1);
    try std.testing.expect(CUM_SCORE >= 1.0);
    try std.testing.expect(RF_CUM_SCORES.get(rf.*).? >= 0.2);
    try std.testing.expect(RF_CUM_SCORES.get(rf2.*).? >= 0.8);

    try std.testing.expect(RF_CUM_SCORES.get(rf2.*).? > 1 or RF_CUM_SCORES.get(rf.*).? > 1);
}

export fn empty_sched() *Sched {
    var s = std.heap.c_allocator.create(Sched) catch @panic("OOM");
    s.random_seed = 0;
    s.delay = delay;
    s.dist = Dist.Normal;
    s.avoid_uninit_writes = true;
    s.pos_len = 0;
    s.neg_len = 0;
    s.obligations = null;
    return s;
}

var ALWAYS_RAND = false;

export fn always_rand() void {
    ALWAYS_RAND = true;
}

var NUM_SCHEDS: u64 = 0;
var MAX_DEPTH: ?u16 = null;

export fn mutate(old_sched: *Sched) *Sched {
    var allocator = std.heap.c_allocator;
    NUM_SCHEDS += 1;

    var num_mutations = @max(PRNG.random().uintLessThan(u64, MAX_MULTI_MUTATIONS), 1);
    var new_sched = old_sched;
    var n: u8 = undefined;

    while (num_mutations > 0) {
        var valid = false;
        std.debug.assert(old_sched.len() <= RF_CUM_SCORES.keys().len or old_sched.len() > 0);
        while (!valid) {
            n = PRNG.random().uintLessThan(u8, 4);
            if (n == 0 and new_sched.len() < RF_CUM_SCORES.keys().len and new_sched.len() < (MAX_DEPTH orelse new_sched.len() + 1)) {
                var interm_sched = insert_mutate(new_sched, allocator);
                new_sched = interm_sched;
                valid = true;
            } else if (n == 1 and new_sched.len() > 0 and new_sched.len() < RF_CUM_SCORES.keys().len) {
                new_sched = swap_mutate(new_sched, allocator);
                valid = true;
            } else if (n == 2 and new_sched.len() > 0) {
                new_sched = negate_mutate(new_sched, allocator);
                valid = true;
            }
            if (n > 2 and !NO_POS) {
                new_sched = rand_mutate(new_sched, allocator);
                if (new_sched.*.random_seed % 4 == 0) { // TODO arbitrarily chosen
                    new_sched.*.avoid_uninit_writes = !new_sched.*.avoid_uninit_writes;
                }
                valid = true;
            }
        }
        num_mutations -= 1;
    }

    if (ALWAYS_RAND or n > 2) {
        new_sched = rand_mutate(new_sched, allocator);
        if (new_sched.*.random_seed % 4 == 0) { // TODO arbitrarily chosen
            new_sched.*.avoid_uninit_writes = !old_sched.*.avoid_uninit_writes;
        }
    }

    return new_sched;
}

fn swap_items(items: []ReadFrom, idxA: usize, idxB: usize) void {
    var tmp = items[idxA];
    items[idxA] = items[idxB];
    items[idxB] = tmp;
}

fn delete_mutate(old_sched: *Sched, allocator: std.mem.Allocator) *Sched {
    std.debug.assert(old_sched.len() > 0);
    var idx = rf_idx_sample(old_sched);

    if (idx >= old_sched.pos_len) {
        swap_items(old_sched.obligs(), idx, old_sched.len() - 1);
        old_sched.neg_len -= 1;
        var buf = serialize(old_sched, allocator);
        const new_sched = deserialize_from(buf);
        old_sched.neg_len += 1;
        return new_sched;
    } else {
        var obs = old_sched.obligs();
        swap_items(obs, idx, old_sched.pos_len - 1);
        var dec: bool = false;
        if (old_sched.neg_len > 0) {
            swap_items(obs, old_sched.pos_len - 1, old_sched.len() - 1);
            dec = true;
        }

        old_sched.pos_len -= 1;

        var buf = serialize(old_sched, allocator);
        const new_sched = deserialize_from(buf);

        old_sched.pos_len += 1;
        if (dec) {
            swap_items(obs, old_sched.pos_len - 1, old_sched.len() - 1);
        }

        return new_sched;
    }
}

fn rand_mutate(old_sched: *Sched, allocator: std.mem.Allocator) *Sched {
    var buf = serialize(old_sched, allocator);
    var new_sched = deserialize_from(buf);
    new_sched.random_seed = PRNG.random().uintLessThan(u16, 65000);
    return new_sched;
}

var USE_WEIGHTED = true;

fn insert_mutate(old_sched: *Sched, allocator: std.mem.Allocator) *Sched {
    std.debug.assert(old_sched.pos_len + old_sched.neg_len < RF_CUM_SCORES.keys().len);
    var ii: usize = 0;
    while (ii < old_sched.len()) {
        std.debug.assert(old_sched.obligations.?[ii].r != 12297829382473034410);
        ii += 1;
    }

    var buf = allocator
        .alignedAlloc(u8, @alignOf(ReadFrom), old_sched.size_bytes() + @sizeOf(ReadFrom)) catch @panic("OOM");
    serialize_to(old_sched.*, buf);
    var new_sched = deserialize_from(buf);
    var obl = std.mem.bytesAsSlice(ReadFrom, buf[@sizeOf(Sched)..buf.len]);
    if (obl.len > 0) {
        new_sched.obligations = obl.ptr;
    }

    var new_rf: ReadFrom = undefined;

    var is_pos = PRNG.random().uintLessThan(u8, 9) < 1;
    if (USE_WEIGHTED and !is_pos) {
        new_rf = weighted_sample(RF_CUM_SCORES);
    } else {
        new_rf = pooled_sample();
    }

    while (!check_is_valid(new_rf, new_sched)) {
        is_pos = PRNG.random().uintLessThan(u8, 9) < 1;
        if (USE_WEIGHTED and !is_pos) {
            new_rf = weighted_sample(RF_CUM_SCORES);
        } else {
            new_rf = pooled_sample();
        }
    }

    if (new_sched.len() == 0) {
        new_sched.obligations.?[0] = new_rf;
        switch (is_pos) {
            true => new_sched.pos_len += 1,
            false => new_sched.neg_len += 1,
        }
        return new_sched;
    }
    if (is_pos and new_sched.neg_len > 0) {
        var tmp = new_sched.obligations.?[new_sched.pos_len];
        new_sched.obligations.?[new_sched.pos_len] = new_rf;
        new_sched.obligations.?[new_sched.neg_len + new_sched.pos_len] = tmp;
        new_sched.pos_len += 1;
    } else if (is_pos) {
        new_sched.obligations.?[new_sched.pos_len] = new_rf;
    } else {
        new_sched.obligations.?[new_sched.neg_len + new_sched.pos_len] = new_rf;
        new_sched.neg_len += 1;
    }

    var i: usize = 0;
    while (i < new_sched.len()) {
        std.debug.assert(new_sched.obligations.?[i].r != 12297829382473034410);
        i += 1;
    }
    return new_sched;
}

fn swap_mutate(old_sched: *Sched, allocator: std.mem.Allocator) *Sched {
    std.debug.assert(old_sched.pos_len + old_sched.neg_len < RF_CUM_SCORES.keys().len);
    var buf = serialize(old_sched, allocator);
    var new_sched = deserialize_from(buf);
    if (new_sched.pos_len + new_sched.neg_len == 0) {
        return new_sched;
    }

    var idx = rf_idx_sample(new_sched);
    var new_is_pos = idx >= old_sched.pos_len;
    var new_rf: ReadFrom = undefined;
    if (USE_WEIGHTED and !new_is_pos) {
        new_rf = weighted_sample(RF_CUM_SCORES);
    } else {
        new_rf = pooled_sample();
    }
    while (!check_is_valid(new_rf, new_sched)) {
        idx = rf_idx_sample(new_sched);
        new_is_pos = idx >= old_sched.pos_len;
        if (USE_WEIGHTED and !new_is_pos) {
            new_rf = weighted_sample(RF_CUM_SCORES);
        } else {
            new_rf = pooled_sample();
        }
    }
    new_sched.obligations.?[idx] = new_rf;
    return new_sched;
}

fn negate_mutate(old_sched: *Sched, allocator: std.mem.Allocator) *Sched {
    var buf = serialize(old_sched, allocator);
    var new_sched = deserialize_from(buf);
    if (new_sched.pos_len + new_sched.neg_len == 0) {
        return new_sched;
    }
    var idx = rf_idx_sample(new_sched);
    negate(new_sched, idx);
    return new_sched;
}

fn negate(sched: *Sched, idx: usize) void {
    if (idx < sched.pos_len) {
        var pos = sched.pos_obligs();
        const tmp = pos[idx];
        pos[idx] = pos[pos.len - 1];
        pos[pos.len - 1] = tmp;
        sched.pos_len -= 1;
        sched.neg_len += 1;
    } else {
        var neg = sched.neg_obligs();
        const tmp = neg[idx];
        neg[idx] = neg[neg.len - 1];
        neg[neg.len - 1] = tmp;
        sched.neg_len -= 1;
        sched.pos_len += 1;
    }
}

fn check_is_valid(rf: ReadFrom, sched: *Sched) bool {
    return !in(rf, sched.neg_obligs()) and !in(rf, sched.pos_obligs());
}

fn in(rf: ReadFrom, rfs: []ReadFrom) bool {
    for (rfs) |rf_have| {
        if (rf.w == rf_have.w and rf.r == rf_have.r) {
            return true;
        }
    }
    return false;
}

var PRNG = std.rand.DefaultPrng.init(1);

export fn init_prng(seed: u64) void {
    PRNG = std.rand.DefaultPrng.init(seed);
}

var delay: u16 = 1;
export fn use_delay(new_delay: u16) void {
    delay = new_delay;
}

export fn use_non_weighted_sample() void {
    USE_WEIGHTED = false;
}

export fn use_l2() void {
    L2 = true;
}

var NO_POS = false;
var MAX_MULTI_MUTATIONS: u16 = 1;

var NUM_PATHS: u32 = 0;
var PATH_COUNTS: std.AutoHashMap(u32, u32) = std.AutoHashMap(u32, u32).init(std.heap.c_allocator);
var fmt_buf = std.mem.zeroes([13]u8);
var fname_fmt_buf = std.mem.zeroes([64]u8);
export fn store_path_id(path_id: u32) void {
    const maybe_v = PATH_COUNTS.get(path_id);
    if (maybe_v) |v| {
        PATH_COUNTS.put(path_id, v + 1) catch @panic("OOM");
    } else {
        NUM_PATHS += 1;
        PATH_COUNTS.put(path_id, 1) catch @panic("OOM");
        if (RECORD_INCREMENTAL_EXACT_RFS) {
            const timestamp: i64 = std.time.timestamp();
            const fname = std.fmt.bufPrint(&fname_fmt_buf, "/opt/out/path_{}_{}.csv", .{ timestamp, NUM_PATHS }) catch @panic("print err");
            var incr_rfs_file: ?std.fs.File = std.fs.cwd().createFile(fname, .{}) catch @panic("can't open sched count file");
            const s = std.fmt.bufPrint(&fmt_buf, "{}\n", .{path_id}) catch @panic("print err");
            _ = incr_rfs_file.?.write(s) catch @panic("file write err");
        }
    }

    if (RECORD_EXACT_RFS) {
        const s = std.fmt.bufPrint(&fmt_buf, "{}\n", .{path_id}) catch @panic("print err");
        _ = EXACT_RFS_PATH_FILE.?.write(s) catch @panic("file write err");
    }
}

export fn get_path_id_count(path_id: u32) u32 {
    const maybe_v = PATH_COUNTS.get(path_id);
    if (maybe_v) |v| {
        return v;
    } else {
        return 0;
    }
}

export fn total_paths() u32 {
    return NUM_PATHS;
}

var EXACT_RFS_PATH_FILE: ?std.fs.File = null;
var RECORD_EXACT_RFS = false;
var RECORD_INCREMENTAL_EXACT_RFS = false;

export fn initialize_mutator() void {
    if (std.os.getenv("MAX_DEPTH")) |d| {
        MAX_DEPTH = std.fmt.parseInt(u16, d, 10) catch null;
    }

    if (std.os.getenv("GLOBAL_SCHED_MAX")) |d| {
        GLOBAL_SCHED_MAX = std.fmt.parseInt(u32, d, 10) catch null;
    }

    if (std.os.getenv("NO_POS")) |d| {
        NO_POS = 1 == std.fmt.parseInt(u16, d, 10) catch 0;
    }

    if (std.os.getenv("MAX_MULTI_MUTATIONS")) |d| {
        MAX_MULTI_MUTATIONS = std.fmt.parseInt(u16, d, 10) catch 1;
    }

    if (std.os.getenv("RECORD_EXACT_RFS")) |_| {
        RECORD_EXACT_RFS = true;
        EXACT_RFS_PATH_FILE = std.fs.cwd().createFile("paths.csv", .{}) catch @panic("can't open sched count file");
    }

    if (std.os.getenv("RECORD_INCREMENTAL_EXACT_RFS")) |_| {
        RECORD_INCREMENTAL_EXACT_RFS = true;
    }
}

var GLOBAL_SCHED_MAX: ?u32 = null;

export fn write_num_scheds(out_dir: [*c]const u8) void {
    if (NUM_SCHEDS >= GLOBAL_SCHED_MAX orelse (NUM_SCHEDS + 1)) {
        c.exit(0);
    }

    var dir = std.fs.cwd().openDirZ(out_dir, .{}, false) catch @panic("no out dir");
    var file = dir.createFile("num_scheds.txt", .{}) catch @panic("can't open sched count file");
    defer file.close();
    std.fmt.format(file.writer(), "{}\n", .{NUM_SCHEDS}) catch @panic("can't write to sched ct file");
}

fn pooled_sample() ReadFrom {
    const val = PRNG.random().uintLessThan(usize, RF_CUM_SCORES.keys().len);
    return RF_CUM_SCORES.keys()[val];
}

fn rf_idx_sample(sched: *Sched) usize {
    var prng = std.rand.DefaultPrng.init(0);
    const val = prng.random().uintLessThan(usize, sched.pos_len + sched.neg_len);
    return val;
}

var RF_SCORES: std.AutoArrayHashMap(ReadFrom, f32) = std.AutoArrayHashMap(ReadFrom, f32).init(std.heap.c_allocator);
var RF_CUM_SCORES: std.AutoArrayHashMap(ReadFrom, f32) = std.AutoArrayHashMap(ReadFrom, f32).init(std.heap.c_allocator);

test "smoke mutate" {
    const allocator = std.testing.allocator;
    var rf = allocator.alloc(ReadFrom, 3) catch @panic("OOM");
    defer allocator.free(rf);
    rf[0].r = 2;
    rf[0].w = 1;
    rf[1].r = 0;
    rf[1].w = 1;
    rf[2].r = 9;
    rf[2].w = 9;
    var s = Sched{ .avoid_uninit_writes = false, .random_seed = 0, .delay = 1, .dist = Dist.Normal, .pos_len = 2, .neg_len = 0, .obligations = rf[0..1] };

    var rf_scores = std.AutoArrayHashMap(ReadFrom, f32).init(allocator);
    defer rf_scores.deinit();
    rf_scores.put(rf[0], 0.2) catch @panic("OOM");
    rf_scores.put(rf[1], 0.9) catch @panic("OOM");
    rf_scores.put(rf[2], 0.98) catch @panic("OOM");
    CUM_SCORE = 0.98;
    RF_CUM_SCORES = rf_scores;

    const new_sched = swap_mutate(&s, allocator);
    defer new_sched.deinit_flat(allocator);
    try std.testing.expect(new_sched.obligations.?[0].r != s.obligations.?[0].r);

    const neg_sched = negate_mutate(&s, allocator);
    defer neg_sched.deinit_flat(allocator);
    try std.testing.expect(neg_sched.neg_len > 0);

    const ins_sched = insert_mutate(&s, allocator);
    defer ins_sched.deinit_flat(allocator);
    try std.testing.expect(ins_sched.pos_len > s.pos_len or ins_sched.neg_len > s.neg_len);

    s.neg_len = 2;
    s.pos_len = 0;
    var del_sched = delete_mutate(&s, allocator);
    try std.testing.expect(del_sched.neg_len < s.neg_len);

    s.neg_len = 0;
    s.pos_len = 2;
    del_sched.deinit_flat(allocator);
    del_sched = delete_mutate(&s, allocator);
    try std.testing.expect(del_sched.pos_len < s.pos_len);

    s.neg_len = 1;
    s.pos_len = 1;
    del_sched.deinit_flat(allocator);
    del_sched = delete_mutate(&s, allocator);
    defer del_sched.deinit_flat(allocator);
    try std.testing.expect(del_sched.pos_len < s.pos_len or del_sched.neg_len < s.neg_len);

    const r_sched = mutate(&s);
    defer r_sched.deinit_flat(std.heap.c_allocator);
}

pub fn mainX() void {
    const fname = std.os.argv[1];
    read_rf_set(fname);
    const seed = std.os.getenv("SEED");
    const seed_num = std.fmt.parseInt(u64, seed orelse "0", 10) catch @panic("mutator seed should parse to num");
    PRNG.seed(seed_num);

    const sched = insert_mutate(empty_sched(), std.heap.c_allocator);
    defer sched.deinit_flat(std.heap.c_allocator);
    const sched2 = insert_mutate(sched, std.heap.c_allocator);
    defer sched2.deinit_flat(std.heap.c_allocator);
    const sched3 = insert_mutate(sched2, std.heap.c_allocator);
    defer sched3.deinit_flat(std.heap.c_allocator);

    std.debug.print("Generated schedule {}\n", .{sched3});
    const path: []const u8 = "sched.bin"[0..9];
    inner_write_sched(path, sched3, std.heap.c_allocator);
}

fn weighted_sample(rfs: std.AutoArrayHashMap(ReadFrom, f32)) ReadFrom {
    const vals = rfs.values();
    var roll = PRNG.random().float(f32) * CUM_SCORE;

    const index = binary_search_give_index(vals, roll);
    return rfs.keys()[index];
}

fn binary_search_give_index(vals: []const f32, to_find: f32) usize {
    std.debug.assert(vals.len > 0);
    std.debug.assert(to_find < vals[vals.len - 1]);

    var high = vals.len - 1;
    var low: usize = 0;
    var current: usize = (high - low) / 2;

    // invariant 0 <= vals[low] < vals[high] <= vals.len
    while (high != low) {
        if (vals[current] < to_find) {
            low = current + 1;
        } else if (vals[current] >= to_find) {
            high = current;
        }
        current = (high - low) / 2 + low;
    }
    return high;
}

test "bin search" {
    const vals = [_]f32{ 0.1, 1.2 };
    try std.testing.expect(1 == binary_search_give_index(vals[0..vals.len], 1));
    try std.testing.expect(0 == binary_search_give_index(vals[0..vals.len], 0.05));
    const vals_ = [_]f32{0.1};
    try std.testing.expect(0 == binary_search_give_index(vals_[0..vals_.len], 0.05));
    const vals__ = [_]f32{ 0.1, 1.2, 2.9 };
    try std.testing.expect(1 == binary_search_give_index(vals__[0..vals__.len], 1));
    try std.testing.expect(2 == binary_search_give_index(vals__[0..vals__.len], 1.3));
}

test "scoring sampling event set" {
    const allocator = std.testing.allocator;
    RF_SCORES = std.AutoArrayHashMap(ReadFrom, f32).init(allocator);
    defer RF_SCORES.deinit();
    RF_CUM_SCORES = std.AutoArrayHashMap(ReadFrom, f32).init(allocator);
    defer RF_CUM_SCORES.deinit();

    _ = inner_read_rf_set2("example_event_pairs.json", allocator);
    const rf = weighted_sample(RF_CUM_SCORES);
    _ = rf;
    const rf1 = weighted_sample(RF_CUM_SCORES);
    _ = rf1;
    const rf2 = weighted_sample(RF_CUM_SCORES);
    _ = rf2;
    // std.debug.print("sampled : {} {} {}\n", .{ rf, rf1, rf2 });
}

"use strict";

const log   = e => host.diagnostics.debugLog(e);
const logln = e => log(e + '\n');
const hex   = e => '0x' + e .toString(16);

const is_usermode = e => e.bitwiseShiftRight(63) == 1;

function usage() {
    logln('[bdump] Usage: !bdump "C:\\\\path\\\\to\\\\dump"')
    logln('[bdump] This will create a dump directory and fill it with a memory and register files');
    logln('[bdump] NOTE: you must include the quotes and escape the backslashes!');
}

function __hex_obj(o) {
    for (let k in o) {
        // if limit is in our obj, its almost definitely a seg reg and we
        // should recurse instead of hexing
        // if 0 is in our obj, its probably an array and we should also
        // recurse
        if (typeof o[k] === 'object' && ('limit' in o[k] || 0 in o[k])) {
            __hex_obj(o[k]);
        } else if (typeof o[k] == 'boolean') {
            continue;
        } else {
            o[k] = hex(o[k]);
        }
    }
}

function __create_dir(path) {
    const Control = host.namespace.Debugger.Utility.Control;

    Control.ExecuteCommand('.shell -i- mkdir ' + path);
}

function __rdmsr(msr_num) {
    const Control = host.namespace.Debugger.Utility.Control;

    let line = Control.ExecuteCommand('rdmsr ' + hex(msr_num)).Last();
    // 1: kd> rdmsr 3a
    // msr[3a] = 00000000`00000001
    let chunks = line.split(' ');
    return host.parseInt64(chunks[chunks.length - 1].replace('`', ''), 16);
}

// must run after __collect_segs
function __collect_msrs(regs) {
    const msrs = {
        "tsc":            0x10,
        "apic_base":      0x1b,
        "sysenter_cs":    0x174,
        "sysenter_esp":   0x175,
        "sysenter_eip":   0x176,
        "pat":            0x277,
        "efer":           0xc0000080,
        "star":           0xc0000081,
        "lstar":          0xc0000082,
        "cstar":          0xc0000083,
        "sfmask":         0xc0000084,
        "kernel_gs_base": 0xc0000102,
        "tsc_aux":        0xc0000103,
    };

    for (let msr in msrs) {
        regs[msr] = __rdmsr(msrs[msr]);
    }

    // windbg shows these are zero because it stores the values in the msrs
    const msr_fs_base = 0xc0000100;
    regs.fs.base = __rdmsr(msr_fs_base);
    const msr_gs_base = 0xc0000101;
    regs.gs.base = __rdmsr(msr_gs_base);
}

function __collect_seg(n) {
    const Control = host.namespace.Debugger.Utility.Control;

    let r = {};
    let line = Control.ExecuteCommand('dg ' + n).Last();
    // Sel        Base              Limit          Type    l ze an es ng Flags
    // ---- ----------------- ----------------- ---------- - -- -- -- -- --------
    // 0033 00000000`00000000 00000000`00000000 Code RE Ac 3 Nb By P  Lo 000002fb
    if (line.indexOf('Unable to get descriptor') != -1) {
        logln('[bdump] could not recover ' + n + '!');
        return null;
    }

    let chunks = line.split(' ');

    r.present  = chunks[9] == 'P';
    r.selector = host.parseInt64(chunks[0], 16);
    r.base     = host.parseInt64(chunks[1].replace('`', ''), 16);
    r.limit    = host.parseInt64(chunks[2].replace('`', ''), 16);
    r.attr     = host.parseInt64(chunks[chunks.length - 1].replace('`', ''), 16);

    return r;
}

// must run before __collect_msrs
function __collect_segs(regs) {
    regs.es = __collect_seg('es');
    regs.cs = __collect_seg('cs');
    regs.ss = __collect_seg('ss');
    regs.ds = __collect_seg('ds');
    // these values below will be wrong -- reading msrs will give us the
    // correct ones
    regs.fs = __collect_seg('fs');
    regs.gs = __collect_seg('gs');

    regs.tr = __collect_seg('tr');
    // our ghetto string parsing is wrong for tr, force it to true
    regs.tr.present = true;

    regs.ldtr = __collect_seg('ldtr');
}

function __collect_user(regs) {
    const User = host.currentThread.Registers.User;

    regs.rax = User.rax;
    regs.rbx = User.rbx;
    regs.rcx = User.rcx;
    regs.rdx = User.rdx;
    regs.rsi = User.rsi;
    regs.rdi = User.rdi;
    regs.rip = User.rip;
    regs.rsp = User.rsp;
    regs.rbp = User.rbp;
    regs.r8  = User.r8;
    regs.r9  = User.r9;
    regs.r10 = User.r10;
    regs.r11 = User.r11;
    regs.r12 = User.r12;
    regs.r13 = User.r13;
    regs.r14 = User.r14;
    regs.r15 = User.r15;

    regs.rflags = User.efl;

    if (is_usermode(regs.rip)) {
        regs.dr0 = User.dr0;
        regs.dr1 = User.dr1;
        regs.dr2 = User.dr2;
        regs.dr3 = User.dr3;
        regs.dr6 = User.dr6;
        regs.dr7 = User.dr7;
    }
}

function __collect_fp(regs) {
    const Fprs = host.currentThread.Registers.FloatingPoint;

    regs.fpcw = Fprs.fpcw;
    regs.fpsw = Fprs.fpsw;
    regs.fptw = Fprs.fptw;

    regs.fpst = Array();
    regs.fpst[0] = Fprs.st0;
    regs.fpst[1] = Fprs.st1;
    regs.fpst[2] = Fprs.st2;
    regs.fpst[3] = Fprs.st3;
    regs.fpst[4] = Fprs.st4;
    regs.fpst[5] = Fprs.st5;
    regs.fpst[6] = Fprs.st6;
    regs.fpst[7] = Fprs.st7;
}

function __collect_simd(regs) {
    const Simd = host.currentThread.Registers.SIMD;

    // XXX TODO XXX

    if (is_usermode(regs.rip)) {
        regs.mxcsr = Simd.mxcsr;
    }
}

function __collect_kern(regs) {
    const Kern = host.currentThread.Registers.Kernel;

    regs.cr0 = Kern.cr0;
    regs.cr2 = Kern.cr2;
    regs.cr3 = Kern.cr3;
    regs.cr4 = Kern.cr4;
    regs.cr8 = Kern.cr8;

    regs.xcr0 = Kern.xcr0;

    regs.gdtr = {};
    regs.gdtr.base  = Kern.gdtr;
    regs.gdtr.limit = Kern.gdtl;

    regs.idtr = {};
    regs.idtr.base  = Kern.idtr;
    regs.idtr.limit = Kern.idtl;

    if (!is_usermode(regs.rip)) {
        regs.dr0 = Kern.kdr0;
        regs.dr1 = Kern.kdr1;
        regs.dr2 = Kern.kdr2;
        regs.dr3 = Kern.kdr3;
        regs.dr6 = Kern.kdr6;
        regs.dr7 = Kern.kdr7;

        regs.mxcsr = Kern.kmxcsr;
    }
}

function __fixup_regs(regs) {
    // regs im not sure how to get out of windbg...
    logln("[bdump] don't know how to get mxcsr_mask or fpop, setting to zero...");
    regs.mxcsr_mask = 0;
    regs.fpop       = 0;
    logln('[bdump]');

    logln("[bdump] don't know how to get avx registers, skipping...");
    logln('[bdump]');

    // top 32 bits of tr are incorrectly either 0 or -1
    // take the top bits from the gdtr and OR them in to get a valid tr
    if (regs.tr.base.bitwiseShiftRight(32) == host.Int64(0xffffffff)
        || regs.tr.base.bitwiseShiftRight(32) == host.Int64(0)) {
        logln('[bdump] tr.base is not cannonical...');
        logln('[bdump] old tr.base: ' + hex(regs.tr.base));

        const low32 = host.Int64(0xffffffff);
        let new_tr = regs.tr.base.bitwiseAnd(low32);

        const hi32  = host.parseInt64('0xffffffff00000000', 16);
        const top_bits = regs.gdtr.base.bitwiseAnd(hi32);

        new_tr = new_tr.bitwiseOr(top_bits);
        regs.tr.base = new_tr;

        logln('[bdump] new tr.base: ' + hex(regs.tr.base));
        logln('[bdump]');
    }

    // need to make sure cs has that magic value set?
    const flag = host.Int64(0x2000);
    if (regs.cs.attr.bitwiseAnd(flag) != flag) {
        logln('[bdump] setting flag 0x2000 on cs.attr...');
        logln('[bdump] old cs.attr: ' + hex(regs.cs.attr));
        regs.cs.attr = regs.cs.attr.bitwiseOr(flag);
        logln('[bdump] new cs.attr: ' + hex(regs.cs.attr));
        logln('[bdump]');
    }

    // kernel/user gs can be swapped when reading from msrs
    if (is_usermode(regs.rip) != is_usermode(regs.gs.base)) {
        logln("[bdump] rip and gs don't match kernel/user, swapping...");
        const tmp = regs.kernel_gs_base;
        regs.kernel_gs_base = regs.gs.base;
        regs.gs.base = tmp;
        logln('[bdump] rip: ' + hex(regs.rip));
        logln('[bdump] new gs.base: ' + hex(regs.gs.base));
        logln('[bdump] new kernel_gs_base: ' + hex(regs.kernel_gs_base));
        logln('[bdump]');
    }

    if (is_usermode(regs.rip) && regs.cr8 != 0) {
        logln("[bdump] non-zero IRQL in usermode, resetting to zero...");
        regs.cr8 = 0;
    }

    // if es was lost to the void, copy ds
    if (regs.es === null) {
        logln("[bdump] could not recover es, copying ds...");
        regs.es = {};
        Object.assign(regs.es, regs.ds);
        logln('[bdump]');
    }

    // turn everything into strings because javascript
    __hex_obj(regs);
}

function __collect_regs() {
    let regs = {};

    __collect_user(regs);
    __collect_segs(regs);
    __collect_msrs(regs);
    __collect_fp(regs);
    __collect_simd(regs);
    __collect_kern(regs);

    return regs;
}

function __save_regs(path, regs) {
    const Fs = host.namespace.Debugger.Utility.FileSystem;

    const data = JSON.stringify(regs);

    let file   = Fs.CreateFile(path + '\\regs.json');
    let writer = Fs.CreateTextWriter(file);

    writer.WriteLine(data);
    file.Close();
}

function __save_mem(dmp_type, path) {
    const control = host.namespace.Debugger.Utility.Control;
    const options = new Map([
        ['full', '/f'],
        ['active-kernel', '/ka']
    ]);

    const option = options.get(dmp_type);
    if(option == undefined) {
        logln(`[bdump] ${dmp_type} is an unknown type`);
        return;
    }

    for (const line of control.ExecuteCommand(`dump ${option} ${path}\\mem.dmp`)) {
        logln(`[bdump] ${line}`);
    }
}

function __bdump(dmp_type, path) {
    if (path == undefined) {
        usage();
        return;
    }

    logln('[bdump] creating dir...');
    __create_dir(path);

    logln('[bdump] saving regs...');
    const regs = __collect_regs();
    logln('[bdump] register fixups...');
    __fixup_regs(regs);
    __save_regs(path, regs);
    logln('[bdump] saving mem, get a coffee or have a smoke, this will probably take around 10-15 minutes...');
    __save_mem(dmp_type, path);

    logln('[bdump] done!');
}

function __bdump_full(path) {
    return __bdump('full', path);
}

function __bdump_active_kernel(path) {
    return __bdump('active-kernel', path);
}

function initializeScript() {
    usage();

    return [
        new host.apiVersionSupport(1, 2),
        new host.functionAlias(__bdump_full, "bdump_full"),

        new host.functionAlias(__bdump_active_kernel, "bdump_active_kernel"),
        new host.functionAlias(__bdump_active_kernel, "bdump"),
    ];
}

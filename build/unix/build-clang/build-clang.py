#!/usr/bin/python2.7
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import os.path
import shutil
import subprocess
import platform
import json
import argparse


def check_run(args):
    r = subprocess.call(args)
    assert r == 0


def run_in(path, args):
    d = os.getcwd()
    os.chdir(path)
    check_run(args)
    os.chdir(d)


def patch(patch, srcdir):
    patch = os.path.realpath(patch)
    check_run(['patch', '-d', srcdir, '-p1', '-i', patch, '--fuzz=0',
               '-s'])


def build_package(package_build_dir, run_cmake, cmake_args):
    if not os.path.exists(package_build_dir):
        os.mkdir(package_build_dir)
    if run_cmake:
        run_in(package_build_dir, ["cmake"] + cmake_args)
    run_in(package_build_dir, ["ninja", "install"])


def with_env(env, f):
    old_env = os.environ.copy()
    os.environ.update(env)
    f()
    os.environ.clear()
    os.environ.update(old_env)


def build_tar_package(tar, name, base, directory):
    name = os.path.realpath(name)
    run_in(base, [tar, "-cjf", name, directory])


def svn_co(url, directory, revision):
    check_run(["svn", "co", "-r", revision, url, directory])


def svn_update(directory, revision):
    run_in(directory, ["svn", "update", "-r", revision])


def build_one_stage(env, src_dir, stage_dir, gcc_toolchain_dir, build_libcxx,
                    build_type, assertions):
    def f():
        build_one_stage_aux(src_dir, stage_dir, gcc_toolchain_dir, build_libcxx,
                            build_type, assertions)
    with_env(env, f)


def get_platform():
    p = platform.system()
    if p == "Darwin":
        return "macosx64"
    elif p == "Linux":
        if platform.processor() == "x86_64":
            return "linux64"
        else:
            return "linux32"
    else:
        raise NotImplementedError("Not supported platform")


def is_darwin():
    return platform.system() == "Darwin"


def build_one_stage_aux(src_dir, stage_dir, gcc_toolchain_dir, build_libcxx,
                        build_type, assertions):
    if not os.path.exists(stage_dir):
        os.mkdir(stage_dir)

    build_dir = stage_dir + "/build"
    inst_dir = stage_dir + "/clang"

    run_cmake = True
    if os.path.exists(build_dir):
        run_cmake = False

    cmake_args = ["-GNinja",
                  "-DCMAKE_BUILD_TYPE=%s" % build_type,
                  "-DLLVM_TARGETS_TO_BUILD=X86;ARM",
                  "-DLLVM_ENABLE_ASSERTIONS=%s" % ("ON" if assertions else "OFF"),
                  "-DPYTHON_EXECUTABLE=/usr/local/bin/python2.7",
                  "-DCMAKE_INSTALL_PREFIX=%s" % inst_dir,
                  "-DGCC_INSTALL_PREFIX=%s" % gcc_toolchain_dir,
                  "-DLLVM_EXTERNAL_LIBCXX_BUILD=%s" % ("ON" if build_libcxx else "OFF"),
                  src_dir];
    build_package(build_dir, run_cmake, cmake_args)

if __name__ == "__main__":
    # The directories end up in the debug info, so the easy way of getting
    # a reproducible build is to run it in a know absolute directory.
    # We use a directory in /builds/slave because the mozilla infrastructure
    # cleans it up automatically.
    base_dir = "/builds/slave/moz-toolchain"

    source_dir = base_dir + "/src"
    build_dir = base_dir + "/build"

    llvm_source_dir = source_dir + "/llvm"
    clang_source_dir = source_dir + "/clang"
    compiler_rt_source_dir = source_dir + "/compiler-rt"
    libcxx_source_dir = source_dir + "/libcxx"

    gcc_dir = "/tools/gcc-4.7.3-0moz1"

    if is_darwin():
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = '10.7'

    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', required=True,
                        type=argparse.FileType('r'),
                        help="Clang configuration file")
    parser.add_argument('--clean', required=False,
                        action='store_true',
                        help="Clean the build directory")

    args = parser.parse_args()
    config = json.load(args.config)

    if args.clean:
        shutil.rmtree(build_dir)
        os.sys.exit(0)

    llvm_revision = config["llvm_revision"]
    llvm_repo = config["llvm_repo"]
    clang_repo = config["clang_repo"]
    compiler_repo = config["compiler_repo"]
    libcxx_repo = config["libcxx_repo"]
    stages = 3
    if "stages" in config:
        stages = int(config["stages"])
        if stages not in (1, 2, 3):
            raise ValueError("We only know how to build 1, 2, or 3 stages")
    build_type = "Release"
    if "build_type" in config:
        build_type = config["build_type"]
        if build_type not in ("Release", "Debug", "RelWithDebInfo", "MinSizeRel"):
            raise ValueError("We only know how to do Release, Debug, RelWithDebInfo or MinSizeRel builds")
    assertions = False
    if "assertions" in config:
        assertions = config["assertions"]
        if assertions not in (True, False):
            raise ValueError("Only boolean values are accepted for assertions.")

    if not os.path.exists(source_dir):
        os.makedirs(source_dir)
        svn_co(llvm_repo, llvm_source_dir, llvm_revision)
        svn_co(clang_repo, clang_source_dir, llvm_revision)
        svn_co(compiler_repo, compiler_rt_source_dir, llvm_revision)
        svn_co(libcxx_repo, libcxx_source_dir, llvm_revision)
        os.symlink("../../clang", llvm_source_dir + "/tools/clang")
        os.symlink("../../compiler-rt",
                   llvm_source_dir + "/projects/compiler-rt")
        os.symlink("../../libcxx",
                   llvm_source_dir + "/projects/libcxx")
        for p in config.get("patches", {}).get(get_platform(), []):
            patch(p, source_dir)
    else:
        svn_update(llvm_source_dir, llvm_revision)
        svn_update(clang_source_dir, llvm_revision)
        svn_update(compiler_rt_source_dir, llvm_revision)
        svn_update(libcxx_source_dir, llvm_revision)

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    stage1_dir = build_dir + '/stage1'
    stage1_inst_dir = stage1_dir + '/clang'

    final_stage_dir = stage1_dir

    if is_darwin():
        extra_cflags = ""
        extra_cxxflags = "-stdlib=libc++"
        extra_cflags2 = ""
        extra_cxxflags2 = "-stdlib=libc++"
        cc = "/usr/bin/clang"
        cxx = "/usr/bin/clang++"
        build_libcxx = True
    else:
        extra_cflags = ""
        extra_cxxflags = ""
        extra_cflags2 = "-static-libgcc"
        extra_cxxflags2 = "-static-libgcc -static-libstdc++"
        cc = gcc_dir + "/bin/gcc"
        cxx = gcc_dir + "/bin/g++"
        build_libcxx = False

    if os.environ.has_key('LD_LIBRARY_PATH'):
        os.environ['LD_LIBRARY_PATH'] = '%s/lib64/:%s' % (gcc_dir, os.environ['LD_LIBRARY_PATH']);
    else:
        os.environ['LD_LIBRARY_PATH'] = '%s/lib64/' % gcc_dir

    build_one_stage(
        {"CC": cc + " %s" % extra_cflags,
         "CXX": cxx + " %s" % extra_cxxflags},
        llvm_source_dir, stage1_dir, gcc_dir, build_libcxx,
        build_type, assertions)

    if stages > 1:
        stage2_dir = build_dir + '/stage2'
        stage2_inst_dir = stage2_dir + '/clang'
        final_stage_dir = stage2_dir
        build_one_stage(
            {"CC": stage1_inst_dir + "/bin/clang %s" % extra_cflags2,
             "CXX": stage1_inst_dir + "/bin/clang++ %s" % extra_cxxflags2},
            llvm_source_dir, stage2_dir, gcc_dir, build_libcxx,
            build_type, assertions)

        if stages > 2:
            stage3_dir = build_dir + '/stage3'
            final_stage_dir = stage3_dir
            build_one_stage(
                {"CC": stage2_inst_dir + "/bin/clang %s" % extra_cflags2,
                 "CXX": stage2_inst_dir + "/bin/clang++ %s" % extra_cxxflags2},
                llvm_source_dir, stage3_dir, gcc_dir, build_libcxx,
                build_type, assertions)

    build_tar_package("tar", "clang.tar.bz2", final_stage_dir, "clang")

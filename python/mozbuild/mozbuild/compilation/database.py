# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This modules provides functionality for dealing with code completion.

import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase
from mozbuild.compilation import util

@CommandProvider
class CompilationDatabase(MachCommandBase):
    """CompilationDatabase commands."""

    @Command('compilation-database', category='devenv',
        description='Generate a compilation database for clang.')
    def compile_db(self):
        from mozbuild.frontend.reader import BuildReader
        from mozbuild.frontend.emitter import TreeMetadataEmitter
        from mozbuild.frontend.data import (
            Sources,
            HostSources,
            UnifiedSources,
            GeneratedSources,
        )
        import json

        if not util.check_top_objdir(self.topobjdir):
            return 1

        # The database we're going to dump out to.
        self._db = []

        # The cache for per-directory flags
        self._flags = {}

        # Iterate through moz.build, dumping all the lines into the db
        reader = BuildReader(self.config_environment)
        emitter = TreeMetadataEmitter(self.config_environment)
        for obj in emitter.emit(reader.read_topsrcdir()):
            obj.ack()
            if isinstance(obj, UnifiedSources):
                # For unified sources, only include the unified source file.
                # Note that unified sources are never used for host sources.
                for f in obj.unified_source_mapping:
                    flags = self._get_dir_flags(obj.objdir)
                    self._build_db_line(obj, self.config_environment, f[0],
                                        flags, False)
            elif isinstance(obj, Sources) or isinstance(obj, HostSources) or \
                 isinstance(obj, GeneratedSources):
                # For other sources, include each source file.
                for f in obj.files:
                    flags = self._get_dir_flags(obj.objdir)
                    self._build_db_line(obj, self.config_environment, f,
                                        flags, isinstance(obj, HostSources))

        # Output the database (a JSON file) to objdir/compile_commands.json
        outputfile = os.path.join(self.topobjdir, 'compile_commands.json')
        with open(outputfile, 'w') as jsonout:
            json.dump(self._db, jsonout, indent=0)

    def _get_dir_flags(self, directory):
        if directory in self._flags:
            return self._flags[directory]

        from mozbuild.util import resolve_target_to_make

        make_dir, make_target = resolve_target_to_make(self.topobjdir, directory)
        if make_dir is None and make_target is None:
            raise Exception('Cannot figure out the make dir and target for ' + directory)

        build_vars = util.get_build_vars(directory, self)

        # We only care about the following build variables.
        for name in ('COMPILE_CFLAGS', 'COMPILE_CXXFLAGS',
                     'COMPILE_CMFLAGS', 'COMPILE_CMMFLAGS'):
            if name not in build_vars:
                continue

            build_vars[name] = util.get_flags(self.topobjdir, directory,
                                              build_vars, name)

        self._flags[directory] = build_vars
        return self._flags[directory]

    def _build_db_line(self, obj, cenv, filename, flags, ishost):
        # Distinguish between host and target files.
        prefix = 'HOST_' if ishost else ''
        if filename.endswith('.c') or filename.endswith('.m'):
            compiler = cenv.substs[prefix + 'CC']
            cflags = flags['COMPILE_CFLAGS']
            # Add the Objective-C flags if needed.
            if filename.endswith('.m'):
                cflags += ' ' + flags['COMPILE_CMFLAGS']
        elif filename.endswith('.cpp') or filename.endswith('.cc') or \
             filename.endswith('.cxx') or filename.endswith('.mm'):
            compiler = cenv.substs[prefix + 'CXX']
            cflags = flags['COMPILE_CXXFLAGS']
            # Add the Objective-C++ flags if needed.
            if filename.endswith('.mm'):
                cflags += ' ' + flags['COMPILE_CMMFLAGS']
        else:
            return
        if not os.path.isfile(filename):
            # First, look for the file in the source directory.
            name = obj.srcdir + '/' + filename
            # Then, look for the file in the object directory (for GeneratedSources).
            if not os.path.isfile(name):
                name = obj.objdir + '/' + filename
            if not os.path.isfile(name):
                raise Exception('Cannot find ' + filename)
            filename = name

        cmd = (
          compiler +
          ' -o /dev/null -c ' + # The output file doesn't matter, so just make something up.
          cflags + ' ' +
          filename
        )

        self._db.append({
            'directory': obj.objdir,
            'command': cmd,
            'file': filename
        })


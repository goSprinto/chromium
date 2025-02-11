# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import optparse
import unittest

from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.controllers.test_result_writer import write_test_result
from blinkpy.web_tests.port.driver import DriverOutput
from blinkpy.web_tests.port.test import TestPort
from blinkpy.web_tests.models import test_failures


class TestResultWriterTests(unittest.TestCase):

    def setUp(self):
        self._actual_output = DriverOutput(
            text='', image=None, image_hash=None, audio=None)
        self._expected_output = DriverOutput(
            text='', image=None, image_hash=None, audio=None)

    def run_test(self, failures=None, files=None, filename='foo.html'):
        failures = failures or []
        host = MockSystemHost()
        host.filesystem.files = files or {}
        port = TestPort(host=host, port_name='test-mac-mac10.11', options=optparse.Values())
        write_test_result(
            host.filesystem, port, '/tmp', filename, self._actual_output,
            self._expected_output, failures)
        return host.filesystem

    def test_success(self):
        # Nothing is written when the test passes.
        fs = self.run_test(failures=[])
        self.assertEqual(fs.written_files, {})

    def test_reference_exists(self):
        failure = test_failures.FailureReftestMismatch(
            self._actual_output, self._expected_output)
        failure.reference_filename = '/src/exists-expected.html'
        files = {'/src/exists-expected.html': 'yup'}
        fs = self.run_test(failures=[failure], files=files)
        self.assertEqual(fs.written_files, {'/tmp/exists-expected.html': 'yup'})

        failure = test_failures.FailureReftestMismatchDidNotOccur(
            self._actual_output, self._expected_output)
        failure.reference_filename = '/src/exists-expected-mismatch.html'
        files = {'/src/exists-expected-mismatch.html': 'yup'}
        fs = self.run_test(failures=[failure], files=files)
        self.assertEqual(fs.written_files, {'/tmp/exists-expected-mismatch.html': 'yup'})

    def test_reference_is_missing(self):
        failure = test_failures.FailureReftestMismatch(
            self._actual_output, self._expected_output)
        failure.reference_filename = 'notfound.html'
        fs = self.run_test(failures=[failure], files={})
        self.assertEqual(fs.written_files, {})

        failure = test_failures.FailureReftestMismatchDidNotOccur(
            self._actual_output, self._expected_output)
        failure.reference_filename = 'notfound.html'
        fs = self.run_test(failures=[failure], files={})
        self.assertEqual(fs.written_files, {})

    def test_reftest_image_missing(self):
        failure = test_failures.FailureReftestNoImageGenerated(
            self._actual_output, self._expected_output)
        failure.reference_filename = '/src/exists-expected.html'
        files = {'/src/exists-expected.html': 'yup'}
        fs = self.run_test(failures=[failure], files=files)
        self.assertEqual(fs.written_files, {'/tmp/exists-expected.html': 'yup'})

        failure = test_failures.FailureReftestNoReferenceImageGenerated(
            self._actual_output, self._expected_output)
        failure.reference_filename = '/src/exists-expected.html'
        files = {'/src/exists-expected.html': 'yup'}
        fs = self.run_test(failures=[failure], files=files)
        self.assertEqual(fs.written_files, {'/tmp/exists-expected.html': 'yup'})

    def test_slash_in_test_name(self):
        failure = test_failures.FailureTestHarnessAssertion(
            self._actual_output, self._expected_output)
        fs = self.run_test(failures=[failure], filename='foo.html?a/b')
        self.assertTrue('/tmp/foo_a_b-actual.txt' in fs.written_files)
        self.assertEqual(set(fs.written_files.keys()), {
            '/tmp/foo_a_b-actual.txt',
            '/tmp/foo_a_b-diff.txt',
            '/tmp/foo_a_b-expected.txt',
            '/tmp/foo_a_b-pretty-diff.html',
        })
        # Should not mkdir '/tmp/foo.html?a'
        self.assertEqual(fs.dirs, {'/', '/tmp'})

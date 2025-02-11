# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Output formatter for JSON Test Results Format.

Format specification:
https://chromium.googlesource.com/chromium/src/+/master/docs/testing/json_test_results_format.md
"""

import calendar
import collections
import datetime
import json
import os
import urllib


OUTPUT_FILENAME = 'test-results.json'


def ProcessIntermediateResults(intermediate_results, options):
  """Process intermediate results and write output in output_dir."""
  results = Convert(intermediate_results, options.output_dir)
  with open(os.path.join(options.output_dir, OUTPUT_FILENAME), 'w') as f:
    json.dump(results, f, sort_keys=True, indent=4, separators=(',', ': '))


def Convert(in_results, base_dir):
  """Convert intermediate results to the JSON Test Results Format.

  Args:
    in_results: The parsed intermediate results.
    base_dir: A string with the path to a base directory; artifact file paths
      will be written relative to this.

  Returns:
    A JSON serializable dict with the converted results.
  """
  results = {'tests': {}}
  status_counter = collections.Counter()

  for result in in_results['testResults']:
    benchmark_name, story_name = result['testPath'].split('/')
    story_name = urllib.unquote(story_name)
    actual_status = result['status']
    expected_status = actual_status if result['isExpected'] else 'PASS'
    status_counter[actual_status] += 1
    artifacts = result.get('outputArtifacts', {})
    shard = _GetTagValue(result.get('tags', []), 'shard', as_type=int)
    _MergeDict(
        results['tests'],
        {
            benchmark_name: {
                story_name: {
                    'actual': actual_status,
                    'expected': expected_status,
                    'is_unexpected': not result['isExpected'],
                    'times': float(result['runDuration'].rstrip('s')),
                    'shard': shard,
                    'artifacts': {
                        name: _ArtifactPath(artifact, base_dir)
                        for name, artifact in artifacts.items()
                    }
                }
            }
        }
    )

  for stories in results['tests'].values():
    for test in stories.values():
      test['actual'] = _DedupedStatus(test['actual'])
      test['expected'] = ' '.join(sorted(set(test['expected'])))
      test['is_unexpected'] = any(test['is_unexpected'])
      test['time'] = test['times'][0]
      test['shard'] = test['shard'][0]  # All shard values should be the same.
      if test['shard'] is None:
        del test['shard']

  benchmark_run = in_results['benchmarkRun']
  results.update(
      seconds_since_epoch=_TimestampToEpoch(benchmark_run['startTime']),
      interrupted=benchmark_run['interrupted'],
      num_failures_by_type=dict(status_counter),
      path_delimiter='/',
      version=3,
  )

  return results


def _DedupedStatus(values):
  # TODO(crbug.com/754825): The following logic is a workaround for how the
  # flakiness dashboard determines whether a test is flaky. As a test_case
  # (i.e. story) may be run multiple times, we squash as sequence of PASS
  # results to a single one. Note this does not affect the total number of
  # passes in num_failures_by_type.
  deduped = set(values)
  if deduped == {'PASS'}:
    return 'PASS'
  elif deduped == {'SKIP'}:
    return 'SKIP'
  else:
    return ' '.join(values)


def _GetTagValue(tags, key, default=None, as_type=None):
  """Get the value of the first occurrence of a tag with a given key."""
  if as_type is None:
    as_type = lambda x: x
  return next((as_type(t['value']) for t in tags if t['key'] == key), default)


def _ArtifactPath(artifact, base_dir):
  """Extract either remote or local path of an artifact."""
  if 'remoteUrl' in artifact:
    return artifact['remoteUrl']
  else:
    # The spec calls for paths to be relative to the output directory and
    # '/'-delimited on all platforms.
    path = os.path.relpath(artifact['filePath'], base_dir)
    return path.replace(os.sep, '/')


def _TimestampToEpoch(timestamp):
  """Convert UTC timestamp to seconds since epoch with microsecond precision."""
  dt = datetime.datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%S.%fZ')
  return calendar.timegm(dt.timetuple()) + dt.microsecond / 1e6


def _MergeDict(target, values):
  # Is used to merge multiple runs of a story into a single test result.
  for key, value in values.items():
    if isinstance(value, dict):
      _MergeDict(target.setdefault(key, {}), value)
    elif isinstance(value, list):
      raise TypeError('Value to merge should not contain lists')
    else:  # i.e. a scalar value.
      target.setdefault(key, []).append(value)

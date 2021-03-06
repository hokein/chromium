# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys
import time

from telemetry.core.util import TimeoutException
from telemetry.page import page_measurement

class RasterizeAndRecordMicro(page_measurement.PageMeasurement):
  def __init__(self):
    super(RasterizeAndRecordMicro, self).__init__('', True)

  def AddCommandLineOptions(self, parser):
    parser.add_option('--start-wait-time', dest='start_wait_time',
                      default=2,
                      help='Wait time before the benchmark is started ' +
                      '(must be long enought to load all content)')
    parser.add_option('--rasterize-repeat', dest='rasterize_repeat',
                      default=100,
                      help='Repeat each raster this many times. Increase ' +
                      'this value to reduce variance.')
    parser.add_option('--record-repeat', dest='record_repeat',
                      default=100,
                      help='Repeat each record this many times. Increase ' +
                      'this value to reduce variance.')
    parser.add_option('--timeout', dest='timeout',
                      default=120,
                      help='The length of time to wait for the micro ' +
                      'benchmark to finish, expressed in seconds.')
    parser.add_option('--report-detailed-results',
                      action='store_true',
                      help='Whether to report additional detailed results.')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-impl-side-painting',
        '--force-compositing-mode',
        '--enable-threaded-compositing',
        '--enable-gpu-benchmarking'
    ])

  def DidStartBrowser(self, browser):
    # Check if the we actually have threaded forced compositing enabled.
    system_info = browser.GetSystemInfo()
    if not (system_info.gpu.feature_status and
            system_info.gpu.feature_status.get(
                'compositing', None) == 'enabled_force_threaded'):
      logging.warning('Warning: compositing feature status unknown or not '+
                      'forced and threaded. Skipping measurement.')
      sys.exit(0)

  def MeasurePage(self, page, tab, results):
    # TODO(vmpstr): Remove this temporary workaround when reference build has
    # been updated to branch 1713 or later.
    backend = tab.browser._browser_backend # pylint: disable=W0212
    if (not hasattr(backend, 'chrome_branch_number') or
        (sys.platform != 'android' and backend.chrome_branch_number < 1713)):
      print ('Warning: rasterize_and_record_micro requires Chrome branch 1713 '
             'or later. Skipping measurement.')
      sys.exit(0)

    try:
      tab.WaitForJavaScriptExpression("document.readyState == 'complete'", 10)
    except TimeoutException:
      pass
    time.sleep(float(self.options.start_wait_time))

    record_repeat = self.options.record_repeat
    rasterize_repeat = self.options.rasterize_repeat
    # Enqueue benchmark
    tab.ExecuteJavaScript("""
        window.benchmark_results = {};
        window.benchmark_results.done = false;
        window.benchmark_results.scheduled =
            chrome.gpuBenchmarking.runMicroBenchmark(
                "rasterize_and_record_benchmark",
                function(value) {
                  window.benchmark_results.done = true;
                  window.benchmark_results.results = value;
                }, {
                  "record_repeat_count": """ + str(record_repeat) + """,
                  "rasterize_repeat_count": """ + str(rasterize_repeat) + """
                });
    """)

    scheduled = tab.EvaluateJavaScript('window.benchmark_results.scheduled')
    if (not scheduled):
      raise page_measurement.MeasurementFailure(
          'Failed to schedule rasterize_and_record_micro')

    tab.WaitForJavaScriptExpression(
        'window.benchmark_results.done', self.options.timeout)

    data = tab.EvaluateJavaScript('window.benchmark_results.results')

    pixels_recorded = data['pixels_recorded']
    record_time = data['record_time_ms']
    pixels_rasterized = data['pixels_rasterized']
    rasterize_time = data['rasterize_time_ms']

    results.Add('pixels_recorded', 'pixels', pixels_recorded)
    results.Add('record_time', 'ms', record_time)
    results.Add('pixels_rasterized', 'pixels', pixels_rasterized)
    results.Add('rasterize_time', 'ms', rasterize_time)

    if self.options.report_detailed_results:
      pixels_rasterized_with_non_solid_color = \
          data['pixels_rasterized_with_non_solid_color']
      pixels_rasterized_as_opaque = \
          data['pixels_rasterized_as_opaque']
      total_layers = data['total_layers']
      total_picture_layers = data['total_picture_layers']
      total_picture_layers_with_no_content = \
          data['total_picture_layers_with_no_content']
      total_picture_layers_off_screen = \
          data['total_picture_layers_off_screen']

      results.Add('pixels_rasterized_with_non_solid_color', 'pixels',
          pixels_rasterized_with_non_solid_color)
      results.Add('pixels_rasterized_as_opaque', 'pixels',
          pixels_rasterized_as_opaque)
      results.Add('total_layers', 'count', total_layers)
      results.Add('total_picture_layers', 'count', total_picture_layers)
      results.Add('total_picture_layers_with_no_content', 'count',
          total_picture_layers_with_no_content)
      results.Add('total_picture_layers_off_screen', 'count',
          total_picture_layers_off_screen)


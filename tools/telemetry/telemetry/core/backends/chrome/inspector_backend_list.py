# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json

from telemetry.core.backends.chrome import inspector_backend


def DebuggerUrlToId(debugger_url):
  return debugger_url.split('/')[-1]


class InspectorBackendList(collections.Sequence):
  """A dynamic sequence of active InspectorBackends."""

  def __init__(self, browser_backend, backend_wrapper=None):
    """Constructor.

    Args:
      browser_backend: The BrowserBackend instance to query for
          InspectorBackends.
      backend_wrapper: A public interface for wrapping each
          InspectorBackend. It must accept a single argument of the
          InspectorBackend to wrap and may expose whatever methods
          are desired on top of that backend.
    """
    self._browser_backend = browser_backend
    # A ordered mapping of context IDs to inspectable contexts.
    self._inspectable_contexts_dict = collections.OrderedDict()
    # A cache of inspector backends, by context ID.
    self._inspector_backend_dict = {}
    # A wrapper class for InspectorBackends.
    self._backend_wrapper = backend_wrapper

  def GetContextInfo(self, context_id):
    return self._inspectable_contexts_dict[context_id]

  def ShouldIncludeContext(self, _context):
    """Override this method to control which contexts are included."""
    return True

  def __getitem__(self, index):
    self._Update()
    context_id = self._inspectable_contexts_dict.keys()[index]
    if context_id not in self._inspector_backend_dict:
      backend = inspector_backend.InspectorBackend(
          self._browser_backend.browser,
          self._browser_backend,
          self._inspectable_contexts_dict[context_id])
      if self._backend_wrapper:
        backend = self._backend_wrapper(backend)
      self._inspector_backend_dict[context_id] = backend
    return self._inspector_backend_dict[context_id]

  def __iter__(self):
    self._Update()
    return self._inspectable_contexts_dict.keys().__iter__()

  def __len__(self):
    self._Update()
    return len(self._inspectable_contexts_dict)

  def _Update(self):
    contexts = json.loads(self._browser_backend.Request(''))
    context_ids = [context['id'] for context in contexts]

    # Append all new inspectable contexts to the dict.
    for context in contexts:
      if not self.ShouldIncludeContext(context):
        continue
      if context['id'] in self._inspectable_contexts_dict:
        continue
      self._inspectable_contexts_dict[context['id']] = context

    # Remove all inspectable contexts that have gone away from the dict.
    for context_id in self._inspectable_contexts_dict.keys():
      if context_id not in context_ids:
        del self._inspectable_contexts_dict[context_id]

    # Clean up any backends for contexts that have gone away.
    for context_id in self._inspector_backend_dict.keys():
      if context_id not in self._inspectable_contexts_dict:
        del self._inspector_backend_dict[context_id]

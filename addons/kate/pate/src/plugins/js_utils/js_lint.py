# -*- coding: utf-8 -*-
# Copyright (c) 2013 by Pablo Martín <goinnn@gmail.com> and
# Alejandro Blanco <alejandro.b.e@gmail.com>
#
# This software is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this software.  If not, see <http://www.gnu.org/licenses/>.

# This file originally was in this repository:
# <https://github.com/goinnn/Kate-plugins/blob/master/kate_plugins/jste_plugins/jslint_plugins.py>


"""
format of errors:
{'a': '===',
 'b': '==',
 'character': 38,
 'evidence': "        return typeof this.headValue == 'undefined';",
 'id': '(error)',
 'line': 16,
 'raw': "Expected '{a}' and instead saw '{b}'.",
 'reason': "Expected '===' and instead saw '=='."}
"""


import re

import kate

from PyKDE4.kdecore import i18n

from jslint.spidermonkey import lint

from js_settings import (KATE_ACTIONS, SETTING_LINT_ON_SAVE)
from libkatepate.errors import (clearMarksOfError, hideOldPopUps,
                                showErrors, showOk)


def lint_js(document, move_cursor=False):
    """Check your js code with the jslint tool"""
    mark_iface = document.markInterface()
    clearMarksOfError(document, mark_iface)
    hideOldPopUps()
    path = document.url().path()
    mark_key = '%s-jslint' % path

    errors = lint(path)
    errors_to_show = []

    # Prepare errors found for painting
    for error in errors:
        if not error:
            continue  # sometimes None
        error['message'] = error.pop('reason')
        error.pop('raw', None)  # Only reason, line, and character are always there
        error.pop('a', None)
        error.pop('b', None)
        errors_to_show.append(error)

    if len(errors_to_show) == 0:
        showOk(i18n("JSLint Ok"))
        return

    showErrors(i18n('JSLint Errors:'),
               errors_to_show,
               mark_key, document,
               key_column='character',
               move_cursor=move_cursor)


@kate.action(**KATE_ACTIONS.lint_JS)
def lint_js_action():
    """Lints the active document"""
    lint_js(kate.activeDocument(), True)


def lint_on_save(document):
    """Tests for multiple Conditions and lints if they are met"""
    if (not document.isModified() and
        document.mimeType() == 'application/javascript' and
        SETTING_LINT_ON_SAVE.lookup()):
        lint_js(document)


@kate.init
@kate.viewCreated
def init(view=None):
    doc = view.document() if view else kate.activeDocument()
    doc.modifiedChanged.connect(lint_on_save)

# kate: space-indent on; indent-width 4;
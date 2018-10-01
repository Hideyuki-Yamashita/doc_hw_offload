#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

import logging
import os
from queue import Queue

# Setup logger object
logger = logging.getLogger(__name__)
# handler = logging.StreamHandler()
os.system("mkdir -p %s/log" % (os.path.dirname(__file__)))

logfile = '%s/log/%s' % (os.path.dirname(__file__), 'spp.log')
handler = logging.FileHandler(logfile)
handler.setLevel(logging.DEBUG)
formatter = logging.Formatter(
    '%(asctime)s,[%(filename)s][%(name)s][%(levelname)s]%(message)s')
handler.setFormatter(formatter)
logger.setLevel(logging.DEBUG)
logger.addHandler(handler)

PRIMARY = ''
SECONDARY_LIST = []

# Initialize primary comm channel
MAIN2PRIMARY = Queue()
PRIMARY2MAIN = Queue()

# Maximum num of sock queues for secondaries
MAX_SECONDARY = 16

# Should be as same as MSG_SIZE in src/shared/common.h
SOCK_BUF_SIZE = 2048

PRIMARY = ''
SECONDARY_COUNT = 0

REMOTE_COMMAND = "RCMD"
RCMD_EXECUTE_QUEUE = Queue()
RCMD_RESULT_QUEUE = Queue()

delim_node = '_'
delim_label = ':'


class GrowingList(list):
    """Growing List

    Custom list type for appending index over the range which is
    similar to ruby's Array. Empty index is filled with 'None'.
    It is used to contain queues for secondaries with any sec ID.

    >>> gl = GrowingList()
    >>> gl.[3] = 0
    >>> gl
    [None, None, None, 0]
    """

    def __setitem__(self, index, value):
        if index >= len(self):
            self.extend([None]*(index + 1 - len(self)))
        list.__setitem__(self, index, value)


# init secondary comm channel list
MAIN2SEC = GrowingList()
SEC2MAIN = GrowingList()

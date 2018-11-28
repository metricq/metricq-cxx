# Copyright (c) 2018, ZIH,
# Technische Universitaet Dresden,
# Federal Republic of Germany
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of metricq nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
from abc import abstractmethod
import asyncio
from time import time
import threading

import aio_pika

from .logging import get_logger
from . import datachunk_pb2
from .rpc import rpc_handler
from .data_client import DataClient
from .datachunk_pb2 import DataChunk
from .source_metric import SourceMetric

logger = get_logger(__name__)


class Source(DataClient):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.metrics = dict()

    async def connect(self):
        await super().connect()
        await self.rpc('source.register', self.handle_register_response)

    @rpc_handler('config')
    async def handle_config(self, **config):
        pass

    async def handle_register_response(self, dataExchange, **response):
        logger.info('register response: {}', response)

        self.data_config(**response)

        self.data_exchange = await self.data_channel.declare_exchange(
            name=dataExchange, passive=True)

        if 'config' in response:
            await self.rpc_dispatch('config', **response['config'])

        await self.ready_callback()

        if hasattr(self, 'run_forever'):
            asyncio.get_event_loop().create_task(self.run_forever())

        #TODO @bmario make a nice timer class

    # TODO can we get rid of this?
    async def ready_callback(self):
        logger.debug('{} ready', self.token)

    def __getitem__(self, id):
        if id not in self.metrics:
            self.metrics[id] = SourceMetric(id, self)
        return self.metrics[id]

    async def declare_metrics(self, metrics):
        logger.debug('declare_metrics({})', metrics)
        await self.rpc('source.declare_metrics', response_callback=None,
                       arguments={'metrics': metrics})

    async def send(self, id, time, value):
        """
        Logical send.
        Dispatches to the SourceMetric for chunking
        """
        logger.debug('send({},{},{})', id, time, value)
        metric = self[id]
        assert metric is not None
        await metric.send(time, value)

    async def _send(self, id, datachunk: DataChunk):
        """
        Actual send of a chunk,
        don't call from anywhere other than SourceMetric
        """
        msg = aio_pika.Message(datachunk.SerializeToString())
        await self.data_exchange.publish(msg, routing_key=id)

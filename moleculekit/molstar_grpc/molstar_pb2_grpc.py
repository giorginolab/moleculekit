# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
"""Client and server classes corresponding to protobuf-defined services."""
import grpc

import .molstar_pb2 as molstar__pb2


class MoleculeLoaderStub(object):
    """Missing associated documentation comment in .proto file."""

    def __init__(self, channel):
        """Constructor.

        Args:
            channel: A grpc.Channel.
        """
        self.LoadMolecule = channel.unary_unary(
                '/molstar.MoleculeLoader/LoadMolecule',
                request_serializer=molstar__pb2.MoleculeData.SerializeToString,
                response_deserializer=molstar__pb2.LoadResponse.FromString,
                )


class MoleculeLoaderServicer(object):
    """Missing associated documentation comment in .proto file."""

    def LoadMolecule(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')


def add_MoleculeLoaderServicer_to_server(servicer, server):
    rpc_method_handlers = {
            'LoadMolecule': grpc.unary_unary_rpc_method_handler(
                    servicer.LoadMolecule,
                    request_deserializer=molstar__pb2.MoleculeData.FromString,
                    response_serializer=molstar__pb2.LoadResponse.SerializeToString,
            ),
    }
    generic_handler = grpc.method_handlers_generic_handler(
            'molstar.MoleculeLoader', rpc_method_handlers)
    server.add_generic_rpc_handlers((generic_handler,))


 # This class is part of an EXPERIMENTAL API.
class MoleculeLoader(object):
    """Missing associated documentation comment in .proto file."""

    @staticmethod
    def LoadMolecule(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/molstar.MoleculeLoader/LoadMolecule',
            molstar__pb2.MoleculeData.SerializeToString,
            molstar__pb2.LoadResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

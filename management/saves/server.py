"""LAN-only HTTP/1.0 save hub. Credentials are supplied from a private file."""
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
from urllib.parse import urlsplit
from .vault import Vault, Conflict, encoded

MAX_BODY = 8 * 1024 * 1024

class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.0'

    def log_message(self, fmt, *args):
        pass  # Never log headers, save contents or authentication material.

    def respond(self, status, value, binary=False):
        data = value if binary else encoded(value)
        self.send_response(status)
        self.send_header('Content-Type', 'application/octet-stream' if binary else 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(data)

    def dispatch(self):
        self.connection.settimeout(10)
        if not hmac.compare_digest(self.headers.get('Authorization', ''), 'Bearer ' + self.server.token):
            self.respond(401, {'error': 'Authentication required'})
            return
        vault = None
        try:
            if self.headers.get('Transfer-Encoding'):
                raise ValueError('Transfer encoding is unsupported')
            length = int(self.headers.get('Content-Length', '0'))
            if not 0 <= length <= MAX_BODY:
                raise ValueError('Request too large')
            body = self.rfile.read(length)
            if len(body) != length:
                raise ValueError('Incomplete request')
            path = urlsplit(self.path).path.split('/')[1:]
            if path[:1] != ['v1']:
                self.respond(404, {'error': 'Unknown API version'})
                return
            path = path[1:]
            vault = Vault(self.server.root)
            if len(path) == 2 and path[0] == 'blobs':
                if self.command == 'PUT':
                    self.respond(200, {'hash': vault.put_blob(path[1], body)})
                    return
                if self.command == 'GET':
                    self.respond(200, vault.get_blob(path[1]), True)
                    return
            value = json.loads(body) if body else {}
            if path == ['campaigns']:
                if self.command == 'GET':
                    self.respond(200, vault.campaigns())
                    return
                if self.command == 'POST':
                    self.respond(201, vault.create(**value))
                    return
            if len(path) == 2 and path[0] == 'campaigns' and self.command == 'GET':
                self.respond(200, vault.campaign(path[1]))
                return
            if len(path) == 3 and path[0] == 'campaigns':
                if path[2] == 'history' and self.command == 'GET':
                    self.respond(200, vault.history(path[1]))
                    return
                if path[2] == 'head' and self.command == 'POST':
                    self.respond(200, {'id': vault.publish(path[1], **value)})
                    return
            if path == ['snapshots'] and self.command == 'POST':
                self.respond(201, {'id': vault.capture(**value)})
                return
            if len(path) == 2 and path[0] == 'snapshots' and self.command == 'GET':
                self.respond(200, vault.snapshot(path[1]))
                return
            self.respond(404, {'error': 'Unknown operation'})
        except Conflict as exc:
            self.respond(409, {'error': 'Campaign changed', 'head': exc.head})
        except (ValueError, TypeError, UnicodeError):
            self.respond(400, {'error': 'Invalid request or damaged content'})
        except (KeyError, FileNotFoundError):
            self.respond(404, {'error': 'Object not found'})
        except OSError:
            self.respond(503, {'error': 'Vault storage unavailable'})
        finally:
            if vault is not None:
                vault.close()

    do_GET = dispatch
    do_POST = dispatch
    do_PUT = dispatch


def serve(root, token_file, bind='127.0.0.1', port=8068):
    secret = Path(token_file)
    if secret.stat().st_mode & 0o077:
        raise ValueError('Token file must be private (mode 0600)')
    token = secret.read_text().strip()
    if len(token) < 32 or not token.isascii() or any(c.isspace() for c in token):
        raise ValueError('Expected a private token of at least 32 ASCII characters')
    Vault(root).close()
    server = ThreadingHTTPServer((bind, port), Handler)
    server.root, server.token = root, token
    try:
        server.serve_forever()
    finally:
        server.server_close()

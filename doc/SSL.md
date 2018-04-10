This guide explains how to [generate self-signed
certificates](http://docs.scylladb.com/operating-scylla/generate_certificate/) for use with ValuStor.

The first step is to create a certificate authority (CA).
```sh
cat << EOF > ca-certificate.cfg
RANDFILE = NV::HOME/.rnd
[ req ]
default_bits = 4096
default_keyfile = <hostname>.key
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no
[ req_distinguished_name ]
C = <country code>
ST = <state>
L = <locality/city>
O = <domain>
OU = <organization, usually domain>
CN= <anything, your name or your company>
emailAddress = <email>
[v3_ca]
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid:always,issuer:always
basicConstraints = CA:true
[v3_req]
# Extensions to add to a certificate request
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
EOF

# Edit the certificate configuration.
vi ca-scylla-certificate.cfg

# Create the certificate authority (CA)
openssl genrsa -out ca-scylla.key 4096
openssl req -x509 -new -nodes -key ca-scylla.key -days 3650 -config ca-scylla-certificate.cfg -out ca-scylla.pem
```

The next step is to generate certificates. Repeat this process once for every ScyllaDB server node (e.g. server1, 
server2, ...) and for every client node (e.g. client1, client2, ...).
```sh
cat << EOF > scylla-server1.cfg
RANDFILE = NV::HOME/.rnd
[ req ]
default_bits = 4096
default_keyfile = <hostname>.key
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no
[ req_distinguished_name ]
C = <country code>
ST = <state>
L = <locality/city>
O = <domain>
OU = <organization, usually domain>
CN= <host>.<domain> or <ip address>
emailAddress = <email>
[v3_ca]
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid:always,issuer:always
basicConstraints = CA:true
[v3_req]
# Extensions to add to a certificate request
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
EOF

# Edit the certificate configuration. Set CN to the host.domain or IP of the ScyllaDB server.
vi scylla-server1.cfg

# Create the server certificate.
openssl genrsa -out scylla-server1.key 4096
openssl req -new -key scylla-server1.key -out scylla-server1.csr -config scylla-server1.cfg
openssl x509 -req -in scylla-server1.csr -CA ca-scylla.pem -CAkey ca-scylla.key -CAcreateserial -out scylla-server1.crt -days 3650
```

Now that the certificates have been generated, ScyllaDB must be configured to use them. Edit the 
`/etc/scylla/scylla.yaml` file on each server and make the following changes:
```
server_encryption_options:
    internode_encryption: all
    certificate: /etc/scylla/scylla-server1.crt
    keyfile: /etc/scylla/scylla-server1.key
    require_client_auth: true
    truststore: /etc/scylla/keys/ca-scylla.pem
    #keystore: conf/.keystore
    #keystore_password: cassandra
    #truststore: conf/.truststore
    #truststore_password: cassandra
    # More advanced defaults below:
    # protocol: TLS
    # algorithm: SunX509
    # store_type: JKS
    # cipher_suites: [TLS_RSA_WITH_AES_128_CBC_SHA,TLS_RSA_WITH_AES_256_CBC_SHA,TLS_DHE_RSA_WITH_AES_128_CBC_SHA,TLS_DHE_RSA_WITH_AES_256_CBC_SHA,TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA]
    # require_client_auth: false

# enable or disable client/server encryption.
client_encryption_options:
    enabled: true
    certificate: /etc/scylla/scylla-server1.crt
    keyfile: /etc/scylla/scylla-server1.key
    require_client_auth: true
    truststore: /etc/scylla/ca-scylla.pem
    #enabled: false
    #keystore: conf/.keystore
    #keystore_password: cassandra
    # require_client_auth: false
    # Set trustore and truststore_password if require_client_auth is true
    # truststore: conf/.truststore
    # truststore_password: cassandra
    # More advanced defaults below:
    # protocol: TLS
    # algorithm: SunX509
    # store_type: JKS
    # cipher_suites: [TLS_RSA_WITH_AES_128_CBC_SHA,TLS_RSA_WITH_AES_256_CBC_SHA,TLS_DHE_RSA_WITH_AES_128_CBC_SHA,TLS_DHE_RSA_WITH_AES_256_CBC_SHA,TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA]
```
Restart the scylla server.

To configure the `cqlsh` client, edit `~/.cassandra/cqlshrc` on each client.
```
[authentication]
username = myusername
password = mypassword
[cql]
; Substitute for the version of Cassandra you are connecting to.
version = 3.3.1
[connection]
hostname = <host>.<domain>
port = 9042
factory = cqlshlib.ssl.ssl_transport_factory
[ssl]
certfile = /etc/scylla/keys/scylla-server.crt
; Note: If validate = true then the certificate name must match the machine's hostname
validate = true
; If using client authentication (require_client_auth = true in cassandra.yaml) you'll also need to point to your userkey $
; SSL client authentication is only supported via cqlsh on C* 2.1 and greater.
; This is disabled by default on all Instaclustr-managed clusters.
userkey = /etc/scylla/scylla-client1.key
usercert = /etc/scylla/scylla-client1.crt
[certfiles]
<host1>.<domain>=/etc/scylla/keys/scylla-server1.crt
<host2>.<domain>=/etc/scylla/keys/scylla-server2.crt
<host3>.<domain>=/etc/scylla/keys/scylla-server3.crt
```

ValuStor can be configured as follows:
```
server_trusted_cert = /etc/scylla/keys/scylla-server1.crt, /etc/scylla/keys/scylla-server2.crt, /etc/scylla/keys/scylla-server3.crt

server_verify_mode = 3 # 0 = None, 1 = Verify Peer Cert, 2 = Verify Peer Identity (IP), 3 = Verify Peer Identity (DNS)

client_ssl_cert = /etc/scylla/keys/scylla-client1.crt
client_ssl_key = /etc/scylla/keys/scylla-client1.key
client_key_password = password
```


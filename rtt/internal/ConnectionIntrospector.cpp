#include "ConnectionIntrospector.hpp"

namespace RTT {
namespace internal {

    ConnectionIntrospector::ConnectionIntrospector(
            const ConnectionManager::ChannelDescriptor& descriptor,
            const PortQualifier& port,
            bool forward, int curr_depth) {
        is_forward = forward;
        connection_id = descriptor.get<0>();
        connection_policy = descriptor.get<2>();
        base::ChannelElementBase::shared_ptr elem = descriptor.get<1>();
        if (is_forward) {
            while (elem && elem->getOutput()) {
                elem = elem->getOutput();
            }
            if (elem->getPort()) {
                in_port = PortQualifier(elem->getPort());
            } else {
                in_port = PortQualifier(elem.get(), true /*is_forward*/);
            }
            out_port = port;
        } else {
            in_port = port;
            while (elem && elem->getInput()) {
                elem = elem->getInput();
            }
            if (elem->getPort()) {
                out_port = PortQualifier(elem->getPort());
            } else {
                out_port = PortQualifier(elem.get(), false /*is_forward*/);
            }
        }
        depth = curr_depth;
    }

    ConnectionIntrospector::ConnectionIntrospector(
            const base::PortInterface* port) {
        PortQualifier port_(port);
        if (port_.is_forward) {
            out_port = port_;
        } else {
            in_port = port_;
        }
        // This is the fake "incoming" port connection, so forward is the
        // opposite of the port value.
        is_forward = !port_.is_forward;
        depth = 0;
    }

    ConnectionIntrospector::ConnectionIntrospector(const TaskContext* tc) {
        if (!tc) {
            in_port.is_remote = true;
            in_port.owner_name = "{EMPTY_COMPONENT}";
            is_forward = true;
            depth = -1;
            return;
        }
        in_port.owner_name = tc->getName();
        is_forward = true;
        depth = -1;
        if (tc->ports() && !(tc->ports()->getPortNames().empty())) {
            DataFlowInterface::PortNames port_names = tc->ports()->getPortNames();
            for (size_t j = 0; j < port_names.size(); ++j) {
                const std::string& port_name = port_names.at(j);
                base::PortInterface* port_ptr = tc->getPort(port_name);
                ConnectionIntrospector ci(port_ptr);
                sub_connections.push_back(ci);
            }
            in_port.is_remote = false;
        } else {
            in_port.is_remote = true;
        }
    }

    bool ConnectionIntrospector::operator==(const ConnectionIntrospector& other)
            const {
        return this->in_port == other.in_port &&
                this->out_port == other.out_port;
    }

    bool ConnectionIntrospector::operator<(const ConnectionIntrospector& other)
            const {
        if (out_port < other.out_port) return true;
        if (other.out_port < out_port) return false;
        return in_port < other.in_port;
    }

    void ConnectionIntrospector::createGraph(int depth) {
        std::list<ConnectionIntrospector*> to_visit;
        if (sub_connections.empty()) {
            to_visit.push_back(this);
        } else {
            for (std::list< ConnectionIntrospector >::iterator
                    it = this->sub_connections.begin();
                    it != this->sub_connections.end(); ++it) {
                to_visit.push_back(&(*it));
            }
        }
        std::set<ConnectionIntrospector> visited;
        createGraph(depth, to_visit, visited);
    }

    void ConnectionIntrospector::addSubConnection(
            const ConnectionIntrospector& other) {
        this->sub_connections.push_back(other);
    }

    void ConnectionIntrospector::createGraph(
            int depth,
            std::list<ConnectionIntrospector*>& to_visit,
            std::set<ConnectionIntrospector>& visited) {
        if (depth < 1) {depth = 1;}

        while (!to_visit.empty()) {
            ConnectionIntrospector& node(*(to_visit.front()));
            to_visit.pop_front();
            int curr_depth = node.depth;
            if (curr_depth >= depth) {
                continue;
            }
            PortQualifier& port = node.is_forward ? node.in_port : node.out_port;

            // Would be undefined for remote ports.
            if (!port.is_remote) {
                std::list<ConnectionIntrospector> ci_list;
                if (!node.sub_connections.empty()) {
                    // Inspecting an already created list, append extra connections.
                    ci_list.insert(ci_list.end(), node.sub_connections.begin(),
                                   node.sub_connections.end());
                    node.sub_connections.clear();
                } else if (port.port_ptr && port.port_ptr->getManager()) {
                    // Perform conversion from ChannelDescriptor.
                    std::list<ConnectionManager::ChannelDescriptor>
                            connections = port.port_ptr->getManager()->getConnections();
                    for (std::list<ConnectionManager::ChannelDescriptor>::const_iterator
                            it = connections.begin(); it != connections.end(); ++it) {
                        ci_list.push_back(ConnectionIntrospector(
                                *it, port, !node.is_forward, curr_depth + 1));
                    }
                }
                // Check whether connections have already been visited,
                // otherwise add them to the to_visit list.
                for (std::list<ConnectionIntrospector>::const_iterator
                        it = ci_list.begin(); it != ci_list.end(); ++it) {
                    if (visited.count(*it)) {
                        continue;
                    }
                    visited.insert(*it);
                    node.sub_connections.push_back(*it);
                    to_visit.push_back(&(node.sub_connections.back()));
                }
            } else {
                if (node.connection_id) {
                    port.port_name = node.connection_id->portName();
                }
            }
        }
    }

    struct ConnectionInfo{
        ConnPolicy policy;
        std::set<ConnectionIntrospector::PortQualifier> inputs;
        std::set<ConnectionIntrospector::PortQualifier> outputs;
    };

    std::ostream& toDotHeader(std::ostream& os) {
        os << "digraph G {\nsize=\"1.2,1\"; ratio=fill; fontsize=45; "
           << "node[fontsize=36]; penwidth=5; edge[penwidth=5]; "
           << "node[penwidth=5]; clusterrank=local;\n\n";
        return os;
    }
    std::ostream& toDotClosing(std::ostream& os) {
        os << "\n}\n";
        return os;
    }

    std::ostream& ConnectionIntrospector::toDot(std::ostream& os) const {
        std::map<std::string, std::set<PortQualifier> > component_to_ports;
        typedef std::string conn_id;
        // Could have a set if multiple connections are still allowed when using
        // a shared policy, but then understanding which conn_id a certain
        // connection corresponds to becomes quite hard.
        std::map<PortQualifier, conn_id> port_to_shared_conn;
        std::map<conn_id, ConnectionInfo> connections;

        std::list<const ConnectionIntrospector*> to_visit;
        std::set<ConnectionIntrospector> visited;
        to_visit.push_back(this);
        {
            // Add name of the first port, if any.
            const PortQualifier& port = this->is_forward ? this->in_port
                                                         : this->out_port;
            if (!port.port_name.empty()) {
                component_to_ports[port.owner_name].insert(port);
            }
        }

        int shared_conn_counter = 0;

        toDotHeader(os);

        while (!to_visit.empty()) {
            const std::list<ConnectionIntrospector>& connection_list = to_visit.front()->sub_connections;
            to_visit.pop_front();
            for (std::list<ConnectionIntrospector>::const_iterator it = connection_list.begin();
                    it != connection_list.end(); ++it) {
                // Push back one connection, and add the node to the "to_visit" list.
                if (visited.count(*it)) {
                    continue;
                }
                visited.insert(*it);
                to_visit.push_back(&(*it));

                const ConnectionIntrospector& node(*it);
                const PortQualifier& port = node.is_forward ? node.in_port : node.out_port;

                // This won't show components with no ports.
                if (!port.port_name.empty()) {
                    component_to_ports[port.owner_name].insert(port);
                }

                // Only positive depth nodes have connection information.
                if (it->depth <= 0) {continue;}

                ostringstream conn_id_stream;
                switch (it->connection_policy.buffer_policy) {
                case PerConnection:
                    // This is uniquely represented, so it needs to be added.
                    conn_id_stream << "PerConnection_" << node.out_port.toDot()
                                   << "_TO_" << node.in_port.toDot();
                    break;
                case PerInputPort:
                    conn_id_stream << "PerInputPort_" << node.in_port.toDot();
                    break;
                case PerOutputPort:
                    conn_id_stream << "PerOutputPort_" << node.out_port.toDot();
                    break;
                case Shared:
                    if (port_to_shared_conn.count(node.in_port) == 0 &&
                            port_to_shared_conn.count(node.out_port) == 0) {
                        // New shared connection.
                        conn_id_stream << "Shared#" << ++shared_conn_counter;
                        port_to_shared_conn[node.in_port] =
                                conn_id_stream.str();
                        port_to_shared_conn[node.out_port] =
                                conn_id_stream.str();
                    } else if (port_to_shared_conn.count(node.in_port) == 0 &&
                               port_to_shared_conn.count(node.out_port) != 0) {
                        // New input port attached to shared connection.
                        port_to_shared_conn[node.in_port] =
                                port_to_shared_conn[node.out_port];

                    } else if (port_to_shared_conn.count(node.in_port) != 0 &&
                               port_to_shared_conn.count(node.out_port) == 0) {
                        // New output port attached to shared connection.
                        port_to_shared_conn[node.out_port] =
                                port_to_shared_conn[node.in_port];
                    } else {
                        // Shared connection already exists, should make sure it
                        // has the same id.
                        if (port_to_shared_conn[node.out_port] !=
                                port_to_shared_conn[node.in_port]) {
                            const std::string& curr_conn_id =
                                    port_to_shared_conn[node.in_port];
                            const std::string conn_id_to_remove =
                                port_to_shared_conn[node.out_port];
                            // Replace the id in all inputs and outputs of the
                            // connection conn_id_to_remove.
                            for (std::set<PortQualifier>::const_iterator it =
                                 connections[conn_id_to_remove].inputs.begin();
                                 it != connections[conn_id_to_remove].inputs.end();
                                 ++it) {
                                port_to_shared_conn[*it] = curr_conn_id;
                            }
                            for (std::set<PortQualifier>::const_iterator it =
                                 connections[conn_id_to_remove].outputs.begin();
                                 it != connections[conn_id_to_remove].outputs.end();
                                 ++it) {
                                port_to_shared_conn[*it] = curr_conn_id;
                            }
                            // Merge connections.
                            connections[curr_conn_id].inputs.insert(
                                    connections[conn_id_to_remove].inputs.begin(),
                                    connections[conn_id_to_remove].inputs.end());
                            connections[curr_conn_id].outputs.insert(
                                    connections[conn_id_to_remove].outputs.begin(),
                                    connections[conn_id_to_remove].outputs.end());
                            connections.erase(conn_id_to_remove);
                        }
                    }
                    break;
                default:
                    assert(false);
                    break;
                }

                if (!conn_id_stream.str().empty()) {
                    conn_id current_conn_id = conn_id_stream.str();
                    connections[current_conn_id].policy = node.connection_policy;
                    connections[current_conn_id].outputs.insert(node.out_port);
                    connections[current_conn_id].inputs.insert(node.in_port);
                }
            }
        }

        for (std::map<std::string, std::set<PortQualifier> >::const_iterator
                it = component_to_ports.begin(); it != component_to_ports.end();
                ++it) {
            os << "subgraph \"cluster_" << it->first << "\" { label=\""
               << it->first << "\"; ";
            for (std::set<PortQualifier>::const_iterator
                    it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                os << it2->toDot() << "; ";
            }
            os << "}\n";
            // Also add single node properties.
            for (std::set<PortQualifier>::const_iterator
                    it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                os << it2->toDot() << " [label=\"" << it2->port_name
                   << "\"];\n";
            }
        }

        for (std::map<conn_id, ConnectionInfo>::const_iterator it = connections.begin();
                it != connections.end(); ++it) {
            conn_id connection_name = it->first;
            // Replace the second whitespace with a pipe.
            ostringstream policy_stream;
            policy_stream << it->second.policy;
            std::string policy_str = policy_stream.str();
            policy_str[policy_str.find(' ', policy_str.find(' ') + 1)] = '|';
            os << connection_name << " [shape=record; label=\"{"
               << connection_name << "|" << policy_str
               << "}\"];\n";
            for (std::set<PortQualifier>::const_iterator
                    it2 = it->second.inputs.begin();
                    it2 != it->second.inputs.end(); ++it2) {
                os << connection_name << " -> " << it2->toDot() << ";\n";
            }
            for (std::set<PortQualifier>::const_iterator
                    it2 = it->second.outputs.begin();
                    it2 != it->second.outputs.end(); ++it2) {
                os << it2->toDot() << " -> " << connection_name << ";\n";
            }
        }

        return toDotClosing(os);
    }

    std::ostream& operator<<(
            std::ostream& os,
            const ConnectionIntrospector::PortQualifier& pq) {
        os << pq.owner_name << "." << pq.port_name;
        return os;
    }

    std::ostream& ConnectionIntrospector::printIndented(std::ostream& os,
            int i) const {
        const int currIndent = 4 * this->depth + i;
        if (this->depth == -1) {
            // For depth -1, only log the component name.
            os << std::string(i, ' ') << this->in_port.owner_name
               << " COMPONENT\n";
        } else if (this->depth == 0) {
            // For depth 0, only log the port.
            std::string connection_summary;
            const int connection_nr = this->sub_connections.size();
            switch (connection_nr) {
            case 0:
                connection_summary = "No";
                break;
            case 1:
                connection_summary = "Single";
                break;
            default:
                connection_summary = "Multiple";
                break;
            }
            os << std::string(currIndent, ' ')
               << (this->is_forward ? " IN " : " OUT ")
               << (this->is_forward ? this->in_port
                                         : this->out_port)
               << " with " << connection_summary << " connection(s) (#"
               << connection_nr << ")" << ":\n";
        } else if (this->depth > 0) {
            // Positive depth, log the full connection information.
            os << std::string(currIndent, ' ')
               << (this->is_forward ? " -> IN " : " <- OUT ")
               << (this->is_forward ? this->in_port
                                         : this->out_port)
               << " : (" << this->connection_id->typeString()
               << " = " << this->connection_policy << ")\n";
        }
        for (std::list< ConnectionIntrospector >::const_iterator
                it = this->sub_connections.begin();
             it != this->sub_connections.end(); ++it) {
            os << *it;
        }
        return os;
    }

    boost::function<std::ostream&(std::ostream&)>
    ConnectionIntrospector::indent(int i) const {
        return boost::bind(&ConnectionIntrospector::printIndented, this, _1, i);
    }

    std::ostream& operator<<(
            std::ostream& os,
            const ConnectionIntrospector& descriptor) {
        return descriptor.indent(0)(os);
    }

}  // namespace internal
}  // namespace RTT

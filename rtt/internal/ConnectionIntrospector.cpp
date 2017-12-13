#include "ConnectionIntrospector.hpp"

namespace RTT {
namespace internal {

    ConnectionIntrospector::ConnectionIntrospector(
            const ConnectionManager::ChannelDescriptor& descriptor,
            bool forward, int curr_depth) {
        is_forward = forward;
        connection_id = descriptor.get<0>();
        connection_policy = descriptor.get<2>();
        in_port = PortQualifier(
                    descriptor.get<1>()->getOutputEndPoint()->getPort());
        out_port = PortQualifier(
                    descriptor.get<1>()->getInputEndPoint()->getPort());
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
        if (tc->ports()) {
            DataFlowInterface::PortNames port_names = tc->ports()->getPortNames();
            for (size_t j = 0; j < port_names.size(); ++j) {
                const std::string& port_name = port_names.at(j);
                base::PortInterface* port_ptr = tc->getPort(port_name);
                ConnectionIntrospector ci(port_ptr);
                sub_connections.push_back(ci);
            }
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
            std::list<ConnectionIntrospector>& connection_list = node.sub_connections;
            PortQualifier& port = node.is_forward ? node.in_port : node.out_port;

            // Would be undefined for remote ports.
            if (!port.is_remote) {
                std::list<ConnectionManager::ChannelDescriptor>
                        connections = port.port_ptr->getManager()->getChannels();
                for (std::list<ConnectionManager::ChannelDescriptor>::const_iterator it = connections.begin();
                     it != connections.end(); ++it) {
                    // Push back one connection, and add the node to the "to_visit" list.
                    ConnectionIntrospector
                            conn_descriptor(*it, !node.is_forward, curr_depth + 1);
                    if (visited.count(conn_descriptor)) {
                        continue;
                    }
                    visited.insert(conn_descriptor);
                    connection_list.push_back(conn_descriptor);
                    to_visit.push_back(&(connection_list.back()));
                }
            } else {
                if (node.connection_id) {
                    port.port_name = node.connection_id->portName();
                }
            }
        }
    }

    void ConnectionIntrospector::toDot() const {
        std::map<std::string, std::vector<std::string> > component_to_ports;
        std::map<std::string, std::string> out_port_to_in_port;
//        std::map<std::string, std::string> out_port_to_connection;
//        std::map<std::string, std::string> connection_to_in_port;

        std::list<const ConnectionIntrospector*> to_visit;
        std::set<ConnectionIntrospector> visited;
        to_visit.push_back(this);

        while (!to_visit.empty()) {
            const ConnectionIntrospector& node(*(to_visit.front()));
            to_visit.pop_front();
            int curr_depth = node.depth;
            const std::list<ConnectionIntrospector>& connection_list = node.sub_connections;
            const PortQualifier& port = node.is_forward ? node.in_port : node.out_port;
            if (curr_depth == -1) {
                component_to_ports[in_port.owner_name];
            }
            for (std::list<ConnectionIntrospector>::const_iterator it = connection_list.begin();
                    it != connection_list.end(); ++it) {
                // Push back one connection, and add the node to the "to_visit" list.
                if (visited.count(*it)) {
                    continue;
                }
                visited.insert(*it);

                component_to_ports[port.owner_name];  // In case it didn't exist
                component_to_ports[port.owner_name].push_back(port.port_name);

                if (depth == 0) {continue;}
                // Make sure pointers exist...
                ostringstream os_port, os_port2;//os_connection;
                os_port << it->out_port;
                os_port2 << it->in_port;
                out_port_to_in_port[os_port.str()] = os_port2.str();
//                os_port << port;
//                // TODO: this needs to have a unique ID!!!
//                os_connection << it->connection_id.typeString() << "|"
//                              << it->connection_policy;
//                if (node.is_forward) {
//                    out_port_to_connection[os_port.toString()] = os_connection.toString();
//                } else {
//                    connection_to_in_port[os_port.toString()] = os_connection.toString();
//                }
                to_visit.push_back(&(*it));
            }
        }

        for (std::map<std::string, std::vector<std::string> >::const_iterator it = component_to_ports.begin();
                 it != component_to_ports.end(); ++it) {
            std::cout << "subgraph \"" << it->first << "\" { label=\""
                      << it->first << "\"; ";
            for (std::vector<std::string>::const_iterator it2 = it->second.begin();
                    it2 != it->second.end(); ++it2) {
                std::cout << *it2 << "; ";
            }
            std::cout << "}\n";
        }
        for (std::map<std::string, std::string>::const_iterator it = out_port_to_in_port.begin();
                it != out_port_to_in_port.end(); ++it) {
            std::cout << it->first << "->" << it->second << "; \n";
        }
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
            os << std::string(currIndent, ' ')
               << (this->is_forward ? " IN " : " OUT ")
               << (this->is_forward ? this->in_port
                                         : this->out_port) << ":\n";
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
        descriptor.toDot();
        return os;
//        return descriptor.indent(0)(os);
    }

}  // namespace internal
}  // namespace RTT

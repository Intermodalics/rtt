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
        if (is_forward) {
            in_port = PortQualifier(
                        descriptor.get<1>()->getOutputEndPoint()->getPort());
            out_port = port;
        } else {
            in_port = port;
            out_port = PortQualifier(
                        descriptor.get<1>()->getInputEndPoint()->getPort());
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

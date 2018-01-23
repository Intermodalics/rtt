#include "ConnectionIntrospector.hpp"

#include "../Port.hpp"
#include "../TaskContext.hpp"

#include <cassert>

namespace RTT {

// forward declaration of corba::RemoteConnID
namespace corba {
   class RemoteConnID;
}

namespace internal {

ConnectionIntrospector::Node::Node(base::ChannelElementBase *endpoint, const std::string &name)
    : direction_(Unspecified), endpoint_(endpoint), name_(name) {}

ConnectionIntrospector::Node::~Node() {}

inline base::ChannelElementBase::shared_ptr ConnectionIntrospector::Node::getEndpoint() const {
    return endpoint_;
}

inline std::string ConnectionIntrospector::Node::getName() const {
    return !name_.empty() ? name_ : "(unnamed)";
}

inline bool ConnectionIntrospector::Node::operator==(const Node& other) const {
    return (endpoint_ == other.endpoint_);
}

inline bool ConnectionIntrospector::Node::operator<(const Node& other) const {
    return (endpoint_ < other.endpoint_);
}

inline ConnectionIntrospector::Node::const_iterator ConnectionIntrospector::Node::begin() const {
    return connections_.begin();
}

inline ConnectionIntrospector::Node::const_iterator ConnectionIntrospector::Node::end() const {
    return connections_.end();
}

ConnectionIntrospector::PortNode::PortNode(const base::PortInterface* port)
    : Node(port->getEndpoint(), port->getQualifiedName()), port_(port) {
    const RTT::base::InputPortInterface *input = dynamic_cast<const RTT::base::InputPortInterface *>(port);
    const RTT::base::OutputPortInterface *output = dynamic_cast<const RTT::base::OutputPortInterface *>(port);
    if (input) {
        direction_ = Input;
    } else if (output) {
        direction_ = Output;
    }
}

inline const base::PortInterface *ConnectionIntrospector::PortNode::getPort() const {
    return port_;
}

std::string ConnectionIntrospector::PortNode::getPortType() const {
    switch(getDirection()) {
        case Input:  return "In";
        case Output: return "Out";
        default:     return "";
    }
}

inline bool ConnectionIntrospector::PortNode::isRemote() const {
    return !(port_->isLocal());
}

ConnectionIntrospector::Connection::Connection(const ConnectionIntrospector::NodePtr &from,
                                               const ConnectionIntrospector::NodePtr &to,
                                               const ConnectionManager::ChannelDescriptor &from_descriptor)
    : from_(from, from_descriptor), to_(to, ConnectionManager::ChannelDescriptor()) {
    if (from->getDirection() == Node::Input) {
        assert(to->getDirection() != Node::Input);
        is_forward_ = false;
    } else if (from->getDirection() == Node::Output) {
        assert(to->getDirection() != Node::Output);
        is_forward_ = true;
    } else if (to->getDirection() == Node::Input) {
        is_forward_ = true;
    } else if (to->getDirection() == Node::Output) {
        is_forward_ = false;
    } else {
        assert(false && "Direction of data flow is not well-defined");
        is_forward_ = true;
    }
}

ConnectionIntrospector::Connection::~Connection() {}

inline ConnectionIntrospector::NodePtr ConnectionIntrospector::Connection::from() const {
    return from_.first.lock();
}

inline ConnectionIntrospector::NodePtr ConnectionIntrospector::Connection::to() const {
    return to_.first.lock();
}

inline ConnectionIntrospector::NodePtr ConnectionIntrospector::Connection::getOutput() const {
    return is_forward_ ? from() : to();
}

inline ConnectionIntrospector::NodePtr ConnectionIntrospector::Connection::getInput() const {
    return is_forward_ ? to() : from();
}

inline bool ConnectionIntrospector::Connection::isForward() const {
    return is_forward_;
}

ConnectionIntrospector::ConnectionIntrospector(const base::PortInterface* port)
    : depth_(0)
{
    add(port);
}

ConnectionIntrospector::ConnectionIntrospector(const DataFlowInterface* interface)
    : depth_(0)
{
    add(interface);
}

ConnectionIntrospector::ConnectionIntrospector(const TaskContext* tc)
    : depth_(0)
{
    add(tc);
}

void ConnectionIntrospector::add(const base::PortInterface* port)
{
    NodePtr node(new PortNode(port));
    start_nodes_.push_back(NodePtr(new PortNode(port)));
}

void ConnectionIntrospector::add(const DataFlowInterface *interface) {
    DataFlowInterface::PortNames ports = interface->getPortNames();
    for(DataFlowInterface::PortNames::const_iterator it = ports.begin();
        it != ports.end(); ++it) {
        NodePtr node(new PortNode(interface->getPort(*it)));
        start_nodes_.push_back(node);
    }
}

void ConnectionIntrospector::add(const TaskContext* tc) {
    return add(tc->ports());
}

void ConnectionIntrospector::reset() {
    nodes_.clear();
    for(Nodes::iterator it = nodes_.begin(); it != nodes_.end(); ++it) {
        (*it)->connections_.clear();
    }
    depth_ = 0;
}

void ConnectionIntrospector::createGraph(int depth) {
    createGraphInternal(depth, start_nodes_);
}

void ConnectionIntrospector::createGraphInternal(int remaining_depth, const Nodes& to_visit) {
    if (remaining_depth < 1) remaining_depth = 1;
    --remaining_depth;
    Nodes next_visit;

//    std::cout << "remaining_depth: " << remaining_depth << " - to_visit:\n";

    for(Nodes::const_iterator node_it = to_visit.begin(); node_it != to_visit.end(); ++node_it) {
        const NodePtr &node = *node_it;

        // If the node represents a port, iterate over all connections:
        PortNode *port_node = dynamic_cast<PortNode *>(node.get());
        if (port_node) {
            ConnectionManager::Connections connections = port_node->getPort()->getManager()->getConnections();
            for(ConnectionManager::Connections::const_iterator con_it = connections.begin();
                con_it != connections.end(); ++con_it) {
                const ConnectionManager::ChannelDescriptor &descriptor = *con_it;
                const base::PortInterface *other_port = descriptor.get<1>()->getPort();
                NodePtr other;
                if (other_port) {
                    other.reset( new PortNode(other_port) );
                } else {
                    other.reset( new Node(descriptor.get<1>().get(), (descriptor.get<0>() ? descriptor.get<0>()->getName() : "")) );
                }

                // Check if the other node is already known
                ConnectionPtr connection;
                NodePtr found = findNode(*other);
                if (found) {
                    // replace other pointer with found node
                    other = found;

                    // try to find matching connection in other where to == node
                    connection = findConnectionTo(other->connections_, *node);
                    if (connection) {
                        // complete descriptor (to part should be empty, or
                        // coherent with what already present)
                        assert(connection->to_.second.get<0>() == 0 ||
                               connection->to_.second.get<0>() ==
                                    descriptor.get<0>());
                        assert(connection->to_.second.get<1>() == 0 ||
                               connection->to_.second.get<1>() ==
                                    descriptor.get<1>());
                        assert(connection->from_.second.get<1>() == node->getEndpoint());
                        connection->to_.second = descriptor;
                    }

                } else {
                    nodes_.push_back(other);
                    if (remaining_depth > 0) {
                        next_visit.push_back(other);
                    }
                }

                // Create new connection if none has been found
                if (!connection.get()) {
                    connection.reset(new Connection(node, other, descriptor));
                }
                node->connections_.push_back(connection);

                // If no further exploration is done on other (final depth) and
                // it is a port, make sure we get the associated descriptor.
                if (remaining_depth <= 0 && other->isPort()) {
                    PortNode *port_node_other = dynamic_cast<PortNode *>(other.get());
                    ConnectionManager::Connections connections_other = port_node_other->getPort()->getManager()->getConnections();
                    for(ConnectionManager::Connections::const_iterator con_other_it = connections_other.begin();
                        con_other_it != connections_other.end(); ++con_other_it) {
                        const ConnectionManager::ChannelDescriptor &descriptor_other = *con_other_it;
                        const base::PortInterface *other_2_port = descriptor_other.get<1>()->getPort();
                        // Only consider the descriptor associated to node.
                        if (!other_2_port || !( PortNode(other_2_port) == *(node.get()) )) {
                            continue;
                        }
                        ConnectionPtr connection_other = findConnectionTo(node->connections_, *other);
                        if (connection_other.get()) {
                            // complete descriptor (to part should be empty, or
                            // coherent with what already present)
                            assert(connection_other->to_.second.get<0>() == 0 ||
                                   connection_other->to_.second.get<0>() ==
                                        descriptor_other.get<0>());
                            assert(connection_other->to_.second.get<1>() == 0 ||
                                   connection_other->to_.second.get<1>() ==
                                        descriptor_other.get<1>());
                            assert(connection_other->from_.second.get<1>() == other->getEndpoint());
                            connection_other->to_.second = descriptor_other;
                            other->connections_.push_back(connection_other);
                        }
                        break;
                    }
                } else if (!other->isPort()) {
                    // For non-port elements, store the same connection to at
                    // least show the connPolicy.
                    other->connections_.push_back(connection);
                }
            }
        }
//        std::cout << " - [" << (node->isPort() ? "" : "NOT ") << "port]"
//                  << node->getName() << "(#" << node->connections_.size() << " connections)" << std::endl;
//        for (ConnectionPtr conn : node->connections_) {
//            std::cout << " -- " << conn->from()->getName() << " > " << conn->to()->getName() << std::endl;
//        }
    }

    if (!next_visit.empty()) {
        createGraphInternal(remaining_depth, next_visit);
    }
}

ConnectionIntrospector::NodePtr ConnectionIntrospector::findNode(const Node &node) const {
    Nodes::const_iterator it;
    for(it = start_nodes_.begin(); it != start_nodes_.end(); ++it) {
        if (**it == node) return *it;
    }
    for(it = nodes_.begin(); it != nodes_.end(); ++it) {
        if (**it == node) return *it;
    }
    return NodePtr();
}

ConnectionIntrospector::ConnectionPtr ConnectionIntrospector::findConnectionTo(const Connections &connections, const Node &node) {
    Connections::const_iterator it;
    for(it = connections.begin(); it != connections.end(); ++it) {
        if (*((*it)->to()) == node) return *it;
    }
    return ConnectionPtr();
}

std::string ConnectionIntrospector::Node::getConnectionSummary() const {
    std::string connection_summary;
    const int connection_nr = this->connections_.size();
    switch (connection_nr) {
    case 0:
        connection_summary = "no";
        break;
    case 1:
        connection_summary = "single";
        break;
    default:
        connection_summary = "multiple";
        break;
    }

    return (" with " + connection_summary + " connection(s) (#"
            + std::to_string(connection_nr) + ")");
}

std::ostream& ConnectionIntrospector::Node::printIndented(
        std::ostream& os, int indent_lvl) const {
    const std::string port_type = "[" +
            (this->isPort()
                ? dynamic_cast<const ConnectionIntrospector::PortNode *>(this)->getPortType()
                : "NOT")
            + " port] ";
    const std::string is_remote =
            this->isRemote() ? "[REMOTE: " + this->endpoint_->getElementName() + "] "
                             : "";

    std::ostringstream debug_info;
    if (this->isRemote() || !this->isPort()) {
        if (this->endpoint_.get()){
            debug_info << "localURI: " << this->endpoint_->getLocalURI()
                      << " | remoteURI: " << this->endpoint_->getRemoteURI();
            if (this->endpoint_->getConnPolicy()) {
                debug_info << " | connPolicy: "
                          << *(this->endpoint_->getConnPolicy());
            } else {
                debug_info << " | connPolicy: NONE";
            }
        } else {
            debug_info << " | endpoint: NONE";
        }
    }

    const std::string connection_summary =
            (indent_lvl == 0 ? this->getConnectionSummary() : "");

    if (indent_lvl == 0) {
        std::cout << "DEBUG CONNECTIONS for " << this->getName() << ":\n";
        for (auto conn: this->connections_) {
            std::cout << " - " << conn->from()->getName() << " > "
                      << conn->to()->getName() << std::endl;
        }
    }

    std::ostringstream connection_str;
    ConnectionPtr connection = findConnectionTo(this->connections_, *this);
    if (connection.get()) {
        connection_str << " [" << connection->to_.second.get<2>() << "]";
    }

    if (connection.get()) {
        if (this->isRemote() || !this->isPort()) {
            if (connection->to_.second.get<0>()) {
                debug_info << " | to_.ConnID.getName: " << connection->to_.second.get<0>()->getName();
            } else {
                debug_info << " | to_.ConnID: NONE";
            }
            if (connection->from_.second.get<0>()) {
                debug_info << " | from_.ConnID.getName: " << connection->from_.second.get<0>()->getName();
            } else {
                debug_info << " | from_.ConnID: NONE";
            }
        }
    }

    const int currIndent = 4 * indent_lvl;
    os << std::string(currIndent, ' ')
       << port_type << is_remote << this->getName() << connection_summary
       << connection_str.str() << "\n";
    if (!debug_info.str().empty()) {
        os << std::string(currIndent, ' ') << "[DEBUG INFO] "
           << debug_info.str() << "\n";
    }

    for (Connections::const_iterator conn = this->connections_.begin();
         conn != this->connections_.end(); ++conn) {
        // Avoid printing myself again.
        if (*((*conn)->to()) == *this) continue;

//        if ((*conn)->to_.second.get<0>()) {
//            os << "[TO] ConnID.getName(): "
//               << (*conn)->to_.second.get<0>()->getName() << std::endl;
//        }
//        if ((*conn)->from_.second.get<0>()) {
//            os << "[FROM] ConnID.getName(): "
//               << (*conn)->from_.second.get<0>()->getName() << std::endl;
//        }
        (*conn)->to()->printIndented(os, indent_lvl + 1);
    }

    return os;
}

std::ostream& operator<<(
        std::ostream& os,
        const ConnectionIntrospector& descriptor) {

    for (ConnectionIntrospector::Nodes::const_iterator node = descriptor.start_nodes_.begin();
         node != descriptor.start_nodes_.end(); ++node) {
        (*node)->printIndented(os, 0) << "\n";
    }

    return os;
}

}  // namespace internal
}  // namespace RTT

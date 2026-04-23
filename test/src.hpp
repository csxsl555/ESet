#ifndef SJTU_ESET_HPP
#define SJTU_ESET_HPP

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace sjtu {

    // Thrown when an invalid iterator operation is performed
    class invalid_iterator : public std::runtime_error {
    public:
        invalid_iterator() : std::runtime_error("Invalid iterator operation!") {}
    };

    template <class Key, class Compare = std::less<Key>>
    class ESet {
    private:
        enum Color { RED,
                     BLACK };

        struct Node {
            Key *data; // Hint: Must use a pointer to avoid requiring a default constructor for Key
            Node *left, *right, *parent;
            Color color;
            size_t sz; // Hint: Maintain subtree size to achieve O(log n) range() queries

            // TODO: Complete the constructors and destructor for Node
            template <typename... Args>
            Node(Node *nil_ptr, Args &&...args)
                : left(nil_ptr), right(nil_ptr), parent(nil_ptr), color(RED), sz(1) {
                data = new Key(std::forward<Args>(args)...);
            }
            Node() : data(nullptr), left(this), right(this), parent(this), color(BLACK), sz(0) {};
            ~Node() {
                if (data)
                    delete data;
            };
        };

        Node *root;
        Node *nil; // Hint: Use a single black sentinel node instead of nullptr
        Compare comp;

        // ================== Internal Helpers & RBT Core Mechanisms ==================

        // TODO: Design any helper functions you need here
        // (e.g., finding min/max in a subtree, recursive copy/destroy, etc.)

        bool is_equal(const Key &a, const Key &b) const { return !comp(a, b) && !comp(b, a); }

        Node *tree_minimum(Node *x) const {
            if (x == nil)
                return nil;
            while (x->left != nil)
                x = x->left;
            return x;
        }

        Node *tree_maximum(Node *x) const {
            if (x == nil)
                return nil;
            while (x->right != nil)
                x = x->right;
            return x;
        }

        void destroy_tree(Node *x) {
            if (x != nil) {
                destroy_tree(x->left);
                destroy_tree(x->right);
                delete x;
            }
        }

        Node *copy_tree(Node *x, Node *parent, Node *other_nil) {
            if (x == other_nil)
                return nil;
            Node *y = new Node(nil, *(x->data)); // copy constructor
            y->color = x->color;
            y->sz = x->sz;
            y->parent = parent;
            y->left = copy_tree(x->left, y, other_nil);
            y->right = copy_tree(x->right, y, other_nil);
            return y;
        }

        // count the elements that less than key
        size_t count_less(const Key &key) const {
            Node *curr = root;
            size_t count = 0;
            while (curr != nil) {
                if (comp(*(curr->data), key)) { // curr < key
                    count += curr->left->sz + 1;
                    curr = curr->right;
                } else {
                    curr = curr->left;
                }
            }
            return count;
        }

        // count the elements that less than or tqual to key
        size_t count_less_or_equal(const Key &key) const {
            Node *curr = root;
            size_t count = 0;
            while (curr != nil) {
                if (comp(key, *(curr->data))) { // curr > key
                    curr = curr->left;
                } else { // curr <= key
                    count += curr->left->sz + 1;
                    curr = curr->right;
                }
            }
            return count;
        }

        void leftRotate(Node *x);
        void rightRotate(Node *y);
        void insertFixup(Node *z);
        void deleteFixup(Node *x);
        void transplant(Node *u, Node *v); // Replaces one subtree as a child of its parent with another subtree

    public:
        // ================== Iterators ==================
        class iterator {
            friend class ESet;

        private:
            Node *node;
            const ESet *tree;

        public:
            iterator() : node(nullptr), tree(nullptr) {}
            iterator(Node *n, const ESet *t) : node(n), tree(t) {}
            iterator(const iterator &other) : node(other.node), tree(other.tree) {}

            const Key &operator*() const {
                if (!tree || node == tree->nil)
                    throw invalid_iterator();
                return *(node->data);
            }
            const Key *operator->() const {
                return &this->operator*();
            }

            iterator &operator++() {
                if (!tree || node == tree->nil) // ++end() do nothing
                    return *this;
                if (node->right != tree->nil) {
                    node = tree->tree_minimum(node->right);
                    return *this;
                } else {
                    Node *tmp = node;
                    while (tmp != tree->root) {
                        if (tmp->parent->left == tmp) {
                            node = tmp->parent;
                            return *this;
                        } else
                            tmp = tmp->parent;
                    }
                    node = tree->nil;
                    return *this;
                }
            }
            iterator operator++(int) {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            iterator &operator--() {
                if (!tree)
                    return *this;
                if (node == tree->nil) { // --end() return the maximum
                    if (tree->root != tree->nil)
                        node = tree->tree_maximum(tree->root);
                    return *this;
                }
                if (node->left != tree->nil) {
                    node = tree->tree_maximum(node->left);
                    return *this;
                } else {
                    Node *tmp = node;
                    while (tmp != tree->root) {
                        if (tmp->parent->right == tmp) {
                            node = tmp->parent;
                            return *this;
                        } else
                            tmp = tmp->parent;
                    }
                    return *this;
                }
            }
            iterator operator--(int) {
                iterator tmp = *this;
                --(*this);
                return tmp;
            }

            bool operator==(const iterator &rhs) const {
                return node == rhs.node && tree == rhs.tree;
            }
            bool operator!=(const iterator &rhs) const {
                return node != rhs.node || tree != rhs.tree;
            }
        };

        using const_iterator = iterator; // Set iterators are inherently read-only

        // ================== Constructors, Destructor & Assignment ==================
        ESet() {
            nil = new Node();
            root = nil;
        }
        ~ESet() {
            destroy_tree(root);
            delete nil;
        }

        ESet(const ESet &other) : comp(other.comp) {
            nil = new Node();
            root = copy_tree(other.root, nil, other.nil);
        }
        ESet &operator=(const ESet &other) {
            if (this == &other)
                return *this;
            destroy_tree(root);
            comp = other.comp;
            root = copy_tree(other.root, nil, other.nil);
            return *this;
        }

        // Move semantics
        ESet(ESet &&other) noexcept : root(other.root), nil(other.nil), comp(std::move(other.comp)) {
            // let other be the empty tree
            other.nil = new Node();
            other.root = other.nil;
        }

        ESet &operator=(ESet &&other) noexcept {
            if (this == &other)
                return *this;
            destroy_tree(root);
            delete nil;

            root = other.root;
            nil = other.nil;
            comp = std::move(other.comp);

            other.nil = new Node();
            other.root = other.nil;
            return *this;
        }

        // ================== Core Interfaces ==================
        size_t size() const noexcept { return root->sz; }

        iterator begin() const noexcept { return iterator(tree_minimum(root), this); }
        iterator end() const noexcept { return iterator(nil, this); }

        // TODO: Pay attention to perfect forwarding and in-place construction
        template <class... Args>
        std::pair<iterator, bool> emplace(Args &&...args);

        size_t erase(const Key &key);

        iterator find(const Key &key) const {
            iterator cur(*root, this);
            while (cur->node != nil) {
                Key *cur_value = cur->node->data;
                if (is_equal(&cur_value, key))
                    return iterator(cur->node, this);
                else if (comp(&cur_value, key))
                    cur = cur->right;
                else
                    cur = cur->left;
            }
            return end();
        }

        iterator lower_bound(const Key &key) const {
            iterator cur(*root, this), cur_ans = end();
            while (cur->node != nil) {
                Key *cur_value = cur->node->data;
                if (is_equal(&cur_value, key))
                    return iterator(cur->node, this);
                else if (comp(&cur_value, key))
                    cur = cur->right;
                else
                    cur_ans = cur, cur = cur->left;
            }
            return iterator(cur_ans, this);
        }
        iterator upper_bound(const Key &key) const {
            iterator cur(*root, this), cur_ans = end();
            while (cur->node != nil) {
                Key *cur_value = cur->node->data;
                if (is_equal(&cur_value, key))
                    cur = cur->left;
                else if (comp(&cur_value, key))
                    cur = cur->right;
                else
                    cur_ans = cur, cur = cur->left;
            }
            return iterator(cur_ans, this);
        }

        // O(log n) range query
        size_t range(const Key &l, const Key &r) const {
            if (comp(r, l))
                return 0; // l > r
            return count_less_or_equal(r) - count_less(l);
        }
    };
} // namespace sjtu

#endif
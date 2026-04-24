#ifndef SJTU_ESET_HPP
#define SJTU_ESET_HPP

#include <algorithm>
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

            // the constructors and destructor for Node
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
        int _size;

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

        void leftRotate(Node *x) {
            Node *y = x->parent, *z = y->parent;
            if (z->left == y)
                z->left = x;
            else
                z->right = x;
            y->parent = x;
            y->right = x->left;
            if (x->left != nil)
                x->left->parent = y;
            y->sz += -(x->sz) + x->left->sz;
            x->left = y;
            x->parent = z;
            x->sz = y->sz + x->right->sz + 1;
        }
        void rightRotate(Node *x) {
            Node *y = x->parent, *z = y->parent;
            if (z->left == y)
                z->left = x;
            else
                z->right = x;
            y->parent = x;
            y->left = x->right;
            if (x->right != nil)
                x->right->parent = y;
            y->sz += -(x->sz) + x->right->sz;
            x->right = y;
            x->parent = z;
            x->sz = y->sz + x->left->sz + 1;
        }
        void insertFixup(Node *z) {
            while (z->parent->color == RED) {
                Node *p = z->parent, *g = p->parent; // z's parent and grandparent
                if (p == g->left) {
                    Node *u = g->right; // z's uncle

                    if (u->color == RED) { // Case 1
                        p->color = BLACK;
                        u->color = BLACK;
                        g->color = RED;
                        z = g;
                    } else {
                        if (z == p->right) { // Case 2
                            leftRotate(p);
                            std::swap(z, p);
                        }
                        // Case 3
                        p->color = BLACK;
                        g->color = RED;
                        rightRotate(g);
                    }
                } else {
                    Node *u = g->left; // z's uncle

                    if (u->color == RED) { // Case 1
                        p->color = BLACK;
                        u->color = BLACK;
                        g->color = RED;
                        z = g;
                    } else {
                        if (z == p->left) { // Case 2
                            rightRotate(p);
                            std::swap(z, p);
                        }
                        // Case 3
                        p->color = BLACK;
                        g->color = RED;
                        leftRotate(g);
                    }
                }
            }

            root->color = BLACK;
        }
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
            _size = 0;
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
        size_t size() const noexcept { return _size; }

        iterator begin() const noexcept { return iterator(tree_minimum(root), this); }
        iterator end() const noexcept { return iterator(nil, this); }

        // Pay attention to perfect forwarding and in-place construction
        template <class... Args>
        std::pair<iterator, bool> emplace(Args &&...args) {
            Node *z = new Node(nil, std::forward<Args>(args)...);
            Node *x = root, *y = nil;
            while (x != nil) {
                y = x;
                if (comp(*(z->data), *(x->data))) {
                    x = x->left;
                } else if (comp(*(x->data), *(z->data))) {
                    x = x->right;
                } else {
                    delete z;
                    return {iterator(x, this), false};
                }
            }

            ++_size;
            z->parent = y;
            if (y == nil) {
                root = z;
            } else if (comp(*(z->data), *(y->data))) {
                y->left = z;
            } else {
                y->right = z;
            }

            Node *cur = y;
            while (cur != nil) {
                ++cur->sz;
                cur = cur->parent;
            }

            insertFixup(z);

            return {iterator(z, this), true};
        }

        size_t erase(const Key &key) {
            --_size;
            return 0;
        }

        iterator find(const Key &key) const {
            Node *cur = root;
            while (cur != nil) {
                if (is_equal(*(cur->data), key))
                    return iterator(cur, this);
                else if (comp(*(cur->data), key))
                    cur = cur->right;
                else
                    cur = cur->left;
            }
            return end();
        }

        iterator lower_bound(const Key &key) const {
            Node *cur = root, *cur_ans = nil;
            while (cur != nil) {
                if (is_equal(*(cur->data), key))
                    return iterator(cur, this);
                else if (comp(*(cur->data), key))
                    cur = cur->right;
                else
                    cur_ans = cur, cur = cur->left;
            }
            return cur_ans == nil ? end() : iterator(cur_ans, this);
        }
        iterator upper_bound(const Key &key) const {
            Node *cur = root, *cur_ans = nil;
            while (cur != nil) {
                if (is_equal(*(cur->data), key))
                    cur = cur->left;
                else if (comp(*(cur->data), key))
                    cur = cur->right;
                else
                    cur_ans = cur, cur = cur->left;
            }
            return cur_ans == nil ? end() : iterator(cur_ans, this);
        }

        // O(log n) range query
        size_t range(const Key &l, const Key &r) const {
            if (comp(r, l))
                return 0; // l > r
            return count_less_or_equal(r) - count_less(l);
        }
    };
} // namespace sjtu
using namespace sjtu;

#endif
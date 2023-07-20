#pragma once
#include "Asg.h"
#include <algorithm>
#include <unordered_set>

SIXPACK_NAMESPACE_BEGIN

/// Abstract Semantic Graph (ASG)
namespace asg {

    // Base Transforms
    //========================================================================================================

    class Identity : public Transform {
    protected:
        std::shared_ptr<const Term> transformImpl(const Sequence& term) {
            auto transformed = std::make_shared<Sequence>();
            for (const auto& t : term.terms()) {
                transformed->addTerm(transform(t));
            }
            return transformed;
        }

        std::shared_ptr<const Term> transformImpl(const Constant& term) { return term.shared_from_this(); }

        std::shared_ptr<const Term> transformImpl(const Input& term) { return term.shared_from_this(); }

        std::shared_ptr<const Term> transformImpl(const Output& term) {
            return std::make_shared<Output>(term.name(), transform(term.term()));
        }

        std::shared_ptr<const Term> transformImpl(const UnaryFunction& term) {
            return std::make_shared<UnaryFunction>(term.function(), transform(term.argument()));
        }

        std::shared_ptr<const Term> transformImpl(const Addition& term) {
            auto transformed = std::make_shared<Addition>(transform(term.constantTerm()));
            for (const auto& t : term.positiveTerms()) {
                transformed->addPositiveTerm(transform(t));
            }
            for (const auto& t : term.negativeTerms()) {
                transformed->addNegativeTerm(transform(t));
            }
            return transformed;
        }

        std::shared_ptr<const Term> transformImpl(const Multiplication& term) {
            auto transformed = std::make_shared<Multiplication>(transform(term.constantTerm()));
            for (const auto& t : term.positiveTerms()) {
                transformed->addPositiveTerm(transform(t));
            }
            for (const auto& t : term.negativeTerms()) {
                transformed->addNegativeTerm(transform(t));
            }
            return transformed;
        }

        std::shared_ptr<const Term> transformImpl(const Exponentiation& term) {
            return std::make_shared<Exponentiation>(transform(term.base()), transform(term.exponent()));
        }

        std::shared_ptr<const Term> transformImpl(const Squaring& term) {
            return std::make_shared<Squaring>(transform(term.base()));
        }

        std::shared_ptr<const Term> coalesceImpl(std::shared_ptr<const Term> term) { return term; }
    };

    class Merge : public Identity {
        std::unordered_map<StringView, std::weak_ptr<const Term>> mTerms;

    protected:
        std::shared_ptr<const Term> coalesceImpl(std::shared_ptr<const Term> term) {
            std::weak_ptr<const Term>& cachedTerm = mTerms[term->key()];
            auto                       uniqueTerm = cachedTerm.lock();
            if (!uniqueTerm) {
                uniqueTerm = std::move(term);
                cachedTerm = uniqueTerm;
            } else if (!uniqueTerm->sourceNode() && term->sourceNode()) {
                const_cast<Term&>(*uniqueTerm).setSourceNode(term->sourceNode());
            }
            return uniqueTerm;
        }
    };

    // Transform Operators
    //========================================================================================================

    template <typename TTransform>
    class TransformOperator : public TTransform {
        static_assert(std::is_base_of_v<Transform, TTransform>);

    protected:
        template <typename T>
        std::shared_ptr<const Term> transformNext(const T& term) {
            return TTransform::transformImpl(term);
        }

    public:
        using TTransform::TTransform;
    };

    template <typename TTransform>
    class ConstEvaluated : public TransformOperator<TTransform> {
    protected:
        std::shared_ptr<const Term> coalesceImpl(std::shared_ptr<const Term> term) {
            if (std::optional<double> constantValue = term->evaluateConstant()) {
                auto constant = std::make_shared<Constant>(*constantValue);
                constant->setSourceNode(term->sourceNode());
                term = std::move(constant);
            }
            return TTransform::coalesceImpl(std::move(term));
        }

    public:
        using TransformOperator<TTransform>::TransformOperator;
    };

    template <typename TTransform>
    class Grouped : public TransformOperator<TTransform> {

        template <typename TOperation>
        std::shared_ptr<const Term> groupTerms(const TOperation& operation) {
            const auto appendTerms = [](auto& to, const auto& from) {
                to.insert(to.end(), from.begin(), from.end());
            };

            Real                                     constantValue = operation.constantTerm()->value();
            std::vector<std::shared_ptr<const Term>> positiveTerms;
            std::vector<std::shared_ptr<const Term>> negativeTerms;
            for (const auto& term : operation.positiveTerms()) {
                auto transformedTerm = this->transform(term);
                if (const auto* constant = dynamic_cast<const Constant*>(transformedTerm.get())) {
                    constantValue = operation.apply(constantValue, constant->value());
                } else if (const auto* sibling = dynamic_cast<const TOperation*>(transformedTerm.get())) {
                    constantValue = operation.apply(constantValue, sibling->constantTerm()->value());
                    appendTerms(positiveTerms, sibling->positiveTerms());
                    appendTerms(negativeTerms, sibling->negativeTerms());
                } else {
                    positiveTerms.push_back(std::move(transformedTerm));
                }
            }
            for (const auto& term : operation.negativeTerms()) {
                auto transformedTerm = this->transform(term);
                if (const auto* constant = dynamic_cast<const Constant*>(transformedTerm.get())) {
                    constantValue = operation.applyInverse(constantValue, constant->value());
                } else if (const auto* sibling = dynamic_cast<const TOperation*>(transformedTerm.get())) {
                    constantValue = operation.applyInverse(constantValue, sibling->constantTerm()->value());
                    appendTerms(positiveTerms, sibling->negativeTerms());
                    appendTerms(negativeTerms, sibling->positiveTerms());
                } else {
                    negativeTerms.push_back(std::move(transformedTerm));
                }
            }
            auto grouped =
                std::make_shared<TOperation>(this->transform(std::make_shared<Constant>(constantValue)));
            for (auto& term : positiveTerms) {
                grouped->addPositiveTerm(std::move(term));
            }
            for (auto& term : negativeTerms) {
                grouped->addNegativeTerm(std::move(term));
            }
            return this->transformNext(*grouped);
        }

    protected:
        using TTransform::transformImpl;

        std::shared_ptr<const Term> transformImpl(const Sequence& term) {
            // Expand nested sequences: (a,b),(c,d) -> a,b,c,d
            auto transformed = std::make_shared<Sequence>();
            for (const auto& t : term.terms()) {
                auto transformedTerm = this->transform(t);
                if (const auto* sequence = dynamic_cast<const Sequence*>(t.get())) {
                    for (const auto& nestedTerm : sequence->terms()) {
                        transformed->addTerm(nestedTerm);
                    }
                } else {
                    transformed->addTerm(std::move(transformedTerm));
                }
            }
            return this->transformNext(*transformed);
        }

        std::shared_ptr<const Term> transformImpl(const Addition& term) {
            // Group terms and constants: (a+2)-(c-(3+b)) -> 5+a+b-c
            return groupTerms(term);
        }

        std::shared_ptr<const Term> transformImpl(const Multiplication& term) {
            // Group terms and constants: (a*2)/(c/(3*b)) -> 5*a*b/c
            return groupTerms(term);
        }

    public:
        using TransformOperator<TTransform>::TransformOperator;
    };

    template <typename TTransform>
    class Reduced : public TransformOperator<TTransform> {

        template <typename TOperation>
        std::shared_ptr<const Term> reduceGroupTerms(const TOperation& operation, const auto& fuseOperator) {
            // Null element constant -> null element.
            if (operation.constantTerm()->value() == operation.nullElement()) {
                return this->transform(operation.constantTerm());
            }
            std::unordered_map<std::shared_ptr<const Term>, int> terms;
            for (const auto& term : operation.positiveTerms()) {
                const auto transformedTerm = this->transform(term);
                ++terms[transformedTerm];
            }
            for (const auto& term : operation.negativeTerms()) {
                const auto transformedTerm = this->transform(term);
                --terms[transformedTerm];
            }
            std::erase_if(terms, [](const auto& keyval) {
                return keyval.second == 0;
            });
            // Single positive term and identity constant -> reduce to the term.
            if (terms.size() == 1 && terms.begin()->second == 1 &&
                operation.constantTerm()->value() == operation.identity()) {
                return terms.begin()->first;
            }
            std::vector<std::shared_ptr<const Term>> positiveTerms;
            std::vector<std::shared_ptr<const Term>> negativeTerms;
            for (const auto& [term, weight] : terms) {
                const int count  = std::abs(weight);
                auto&     output = weight > 0 ? positiveTerms : negativeTerms;
                if (count > 1) {
                    if (auto fusedTerm = fuseOperator(term, count)) {
                        output.push_back(this->transform(std::move(fusedTerm)));
                        continue;
                    }
                }
                for (int i = 0; i < count; ++i) {
                    output.push_back(term);
                }
            }
            // Sort the terms by their key. The shorter terms are placed forth.
            std::sort(positiveTerms.begin(), positiveTerms.end(), [](const auto& t1, const auto& t2) {
                return t1->key().size() == t2->key().size() ? t1->key() < t2->key()
                                                            : t1->key().size() < t2->key().size();
            });
            std::sort(negativeTerms.begin(), negativeTerms.end(), [](const auto& t1, const auto& t2) {
                return t1->key().size() == t2->key().size() ? t1->key() < t2->key()
                                                            : t1->key().size() < t2->key().size();
            });
            auto reduced = std::make_shared<TOperation>(this->transform(operation.constantTerm()));
            for (auto& t : positiveTerms) {
                reduced->addPositiveTerm(std::move(t));
            }
            for (auto& t : negativeTerms) {
                reduced->addNegativeTerm(std::move(t));
            }
            return this->transformNext(*reduced);
        }

    protected:
        using TTransform::transformImpl;

        std::shared_ptr<const Term> transformImpl(const Sequence& term) {
            // Remove duplicate terms from the sequence.
            auto                            transformed = std::make_shared<Sequence>();
            std::unordered_set<const Term*> uniqueTerms;
            for (const auto& t : term.terms()) {
                auto transformedTerm = this->transform(t);
                if (uniqueTerms.insert(transformedTerm.get()).second) {
                    transformed->addTerm(std::move(transformedTerm));
                }
            }
            return this->transformNext(*transformed);
        }

        std::shared_ptr<const Term> transformImpl(const Addition& term) {
            // Reduce identity:     0+a -> a
            // Eliminate terms:     a+b-a -> b
            // Fuse repeated terms: n-times +a ->  n*a
            //                      n-times -a -> -n*a
            return reduceGroupTerms(term, [](const auto& term, int count) {
                auto product = std::make_shared<Multiplication>(std::make_shared<Constant>(count));
                product->addPositiveTerm(term);
                return std::move(product);
            });
        }

        std::shared_ptr<const Term> transformImpl(const Multiplication& term) {
            const auto inverseConstant = [this](const std::shared_ptr<const Constant>& constant) {
                return this->transform(std::make_shared<Constant>(-constant->value()));
            };

            // Transform negative constant to additive inverse: -K*x*(a-b)*(c+d) -> K*x*(b-a)*(c+d)
            if (term.constantTerm()->value() < 0) {
                std::vector<std::shared_ptr<const Term>> positiveTerms;
                std::vector<std::shared_ptr<const Term>> negativeTerms;
                for (const auto& t : term.positiveTerms()) {
                    positiveTerms.push_back(t);
                }
                for (const auto& t : term.negativeTerms()) {
                    negativeTerms.push_back(t);
                }
                std::shared_ptr<const Term>* candidate    = nullptr;
                static constexpr int         UNIQUE_COUNT = 2; // one for our vector and one for actual owner
                for (auto& t : positiveTerms) {
                    if (dynamic_cast<const Addition*>(t.get()) && t.use_count() == UNIQUE_COUNT) {
                        candidate = &t;
                        break;
                    }
                }
                if (!candidate) {
                    for (auto& t : negativeTerms) {
                        if (dynamic_cast<const Addition*>(t.get()) && t.use_count() == UNIQUE_COUNT) {
                            candidate = &t;
                            break;
                        }
                    }
                }
                if (candidate) {
                    const auto& sum     = static_cast<const Addition&>(**candidate);
                    auto        inverse = std::make_shared<Addition>(inverseConstant(sum.constantTerm()));
                    for (const auto& t : sum.positiveTerms()) {
                        inverse->addNegativeTerm(t);
                    }
                    for (const auto& t : sum.negativeTerms()) {
                        inverse->addPositiveTerm(t);
                    }
                    *candidate       = this->transform(inverse);
                    auto transformed = std::make_shared<Multiplication>(inverseConstant(term.constantTerm()));
                    for (const auto& t : positiveTerms) {
                        transformed->addPositiveTerm(t);
                    }
                    for (const auto& t : negativeTerms) {
                        transformed->addNegativeTerm(t);
                    }
                    return transformImpl(*transformed); // recursion
                }
            }

            // Reduce identity:     1*a -> a
            // Reduce null element: 0*a -> 0
            // Eliminate terms:     a*b/a -> b
            // Fuse repeated terms: n-times *a -> a^n
            //                      n-times /a -> a^-n
            return reduceGroupTerms(term, [](const auto& term, int count) {
                return std::make_shared<Exponentiation>(term, std::make_shared<Constant>(count));
            });
        }

        std::shared_ptr<const Term> transformImpl(const Exponentiation& term) {
            // Exponent expansion by recursive squaring: x^7 -> ((x*x)*(x*x))*(x*x)*x
            const auto squaredExponentiation = [](const auto& base, const int exponent) {
                auto result  = std::make_shared<Multiplication>();
                auto current = base;
                for (int bits = std::abs(exponent); bits > 0; bits /= 2) {
                    if (bits & 1) {
                        if (exponent > 0) {
                            result->addPositiveTerm(current);
                        } else {
                            result->addNegativeTerm(current);
                        }
                    }
                    if (bits > 0) {
                        current = std::make_shared<Squaring>(std::move(current));
                    }
                }
                return result;
            };

            if (std::optional<Real> constantExponent = term.exponent()->evaluateConstant()) {
                const int exponent = static_cast<int>(*constantExponent);
                if (static_cast<double>(exponent) == *constantExponent) {
                    return this->transformNext(*squaredExponentiation(term.base(), exponent));
                }
            }
            return this->transformNext(term);
        }

    public:
        using TransformOperator<TTransform>::TransformOperator;
    };

    template <typename TTransform>
    class Renamed : public TransformOperator<TTransform> {
        const std::unordered_map<String, String> mRenames;

        StringView rename(StringView name) const {
            const auto renameIt = mRenames.find(String(name));
            return renameIt != mRenames.end() ? renameIt->second : name;
        }

    protected:
        using TTransform::transformImpl;

        std::shared_ptr<const Term> transformImpl(const Input& term) {
            return this->transformNext(*std::make_shared<Input>(rename(term.name())));
        }

        std::shared_ptr<const Term> transformImpl(const Output& term) {
            return this->transformNext(*std::make_shared<Output>(rename(term.name()), term.term()));
        }

    public:
        using TransformOperator<TTransform>::TransformOperator;

        explicit Renamed(std::unordered_map<String, String> renames)
            : mRenames(std::move(renames)) {}
    };

    template <typename TTransform>
    class TrigonometricIdentities : public TransformOperator<TTransform> {
        std::unordered_map<std::shared_ptr<const Term>, std::shared_ptr<const Term>> mSquaredSines;
        std::unordered_map<std::shared_ptr<const Term>, std::shared_ptr<const Term>> mSquaredCosines;

    protected:
        using TTransform::transformImpl;

        std::shared_ptr<const Term> transformImpl(const Squaring& term) {
            if (const auto* function = dynamic_cast<const UnaryFunction*>(term.base().get())) {
                if (function->function() == RealFunction(&std::sin)) {
                    const auto squaredCosIt = mSquaredCosines.find(function->argument());
                    if (squaredCosIt != mSquaredCosines.end()) {
                        const auto diff = std::make_shared<Addition>(std::make_shared<Constant>(1));
                        diff->addNegativeTerm(squaredCosIt->second);
                        return this->transformNext(*diff);
                    } else {
                        const auto transformed = this->transformNext(term);
                        mSquaredSines.insert({ function->argument(), transformed });
                        return transformed;
                    }
                }
                if (function->function() == RealFunction(&std::cos)) {
                    const auto squaredSinIt = mSquaredSines.find(function->argument());
                    if (squaredSinIt != mSquaredSines.end()) {
                        const auto diff = std::make_shared<Addition>(std::make_shared<Constant>(1));
                        diff->addNegativeTerm(squaredSinIt->second);
                        return this->transformNext(*diff);
                    } else {
                        const auto transformed = this->transformNext(term);
                        mSquaredCosines.insert({ function->argument(), transformed });
                        return transformed;
                    }
                }
            }
            return this->transformNext(term);
        }

    public:
        using TransformOperator<TTransform>::TransformOperator;
    };

} // namespace asg

SIXPACK_NAMESPACE_END

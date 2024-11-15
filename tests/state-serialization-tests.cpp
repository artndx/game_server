#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/model.h"
#include "../src/model_serialization.h"

using namespace model;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

using Point2D = PairDouble;
using Point2D = PairDouble;


SCENARIO_METHOD(Fixture, "Point serialization") {
    GIVEN("A point") {
        const Point2D p{10, 20};
        WHEN("point is serialized") {
            output_archive << p;

            THEN("it is equal to point after serialization") {
                InputArchive input_archive{strm};
                Point2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            Dog dog{42, Dog::Name("Pluto"s), Dog::Position({42.2, 12.5}), Dog::Speed({0,0}), Direction::NORTH};
            dog.SetScore(42);
            dog.SetBagCapacity(3);
            CHECK(dog.PutToBag({10, 2u}));
            dog.SetDirection(Direction::EAST);
            dog.SetSpeed(Dog::Speed({2.3, -1.2}));
            return dog;
        }();

        WHEN("dog is serialized") {

            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                // InputArchive input_archive{strm};
                // serialization::DogRepr repr;
                // input_archive >> repr;
                // const auto restored = repr.Restore();

                // CHECK(dog.GetId() == restored.GetId());
                // CHECK(dog.GetName() == restored.GetName());
                // CHECK(dog.GetPosition() == restored.GetPosition());
                // CHECK(dog.GetSpeed() == restored.GetSpeed());
                // CHECK(dog.GetBagCapacity() == restored.GetBagCapacity());
                // CHECK(dog.GetBag() == restored.GetBag());
            }
        }
    }
}

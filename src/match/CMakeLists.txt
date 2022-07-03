target_sources(
  detfm
  PRIVATE Action.cpp
          ClassMatcher.cpp
          errors.cpp
          MatchResult.cpp
          MethodMatcher.cpp
          MultinameMatcher.cpp
          NumberMatcher.cpp)
add_subdirectory(traits)

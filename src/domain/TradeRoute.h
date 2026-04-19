#pragma once
// domain/TradeRoute.h – Trade-Route-Modell
// TODO Phase 11
#include <QString>
namespace flatlas::domain {
struct Commodity { QString nickname; int idsName = 0; };
struct BaseMarketEntry { QString base; QString commodity; int price = 0; bool sells = false; };
struct TradeRouteCandidate { QString fromBase; QString toBase; QString commodity; int profit = 0; int jumps = 0; };
} // namespace flatlas::domain

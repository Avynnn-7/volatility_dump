
import json
from datetime import datetime

try:
    import yfinance as yf
except ImportError:
    print("Run: pip install yfinance"); exit(1)

spy  = yf.Ticker("SPY")
spot = float(spy.fast_info.last_price)
print(f"SPY spot: {spot:.2f}")

quotes = []
today  = datetime.today()

for exp_str in spy.options[:6]:
    T = max((datetime.strptime(exp_str, "%Y-%m-%d") - today).days / 365.0, 1/365)
    chain = spy.option_chain(exp_str).calls
    chain = chain[(chain['bid'] > 0) & (chain['ask'] > 0)]
    chain = chain[(chain['strike'] >= spot*0.80) & (chain['strike'] <= spot*1.20)]
    for _, row in chain.iterrows():
        iv = float(row['impliedVolatility'])
        if 0.01 < iv < 3.0:
            quotes.append({
                "strike": float(row['strike']),
                "expiry": round(T, 6),
                "iv":     round(iv, 6)
            })

with open("data/spy_quotes.json", "w") as f:
    json.dump({"spot": round(spot, 2), "quotes": quotes}, f, indent=2)

print(f"Saved {len(quotes)} quotes → data/spy_quotes.json")
print(r"Run: .\Release\vol_arb.exe data\spy_quotes.json")

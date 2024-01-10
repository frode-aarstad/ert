

from typing import Optional, Dict
from ert.config.enkf_observation_implementation_type import EnkfObservationImplementationType
from ert.storage import StorageReader, open_storage, EnsembleReader
from ert.libres_facade import LibresFacade
import pandas as pd
import time


def data_for_key(
    ensemble: EnsembleReader,
    key: str,
    realization_index: Optional[int] = None,
) -> pd.DataFrame:
    """Returns a pandas DataFrame with the datapoints for a given key for a
    given case. The row index is the realization number, and the columns are an
    index over the indexes/dates"""

    if key.startswith("LOG10_"):
        key = key[6:]
    if key in ensemble.get_summary_keyset():
        data = ensemble.load_summary(key)
        data = data[key].unstack(level="Date")
        print(f"Done-2")
    elif key in ensemble.get_gen_kw_keyset():
        data = ensemble.load_all_gen_kw_data(key.split(":")[0], realization_index)
        if data.empty:
            return pd.DataFrame()
        data = data[key].to_frame().dropna()
        data.columns = pd.Index([0])
        print(f"Done-3")
    elif key in ensemble.get_gen_data_keyset():
        key_parts = key.split("@")
        key = key_parts[0]
        report_step = int(key_parts[1]) if len(key_parts) > 1 else 0
        print(f"Done-3")
        try:
            data = ensemble.load_gen_data(
                key,
                report_step,
                realization_index,
            ).T
        except (ValueError, KeyError):
            return pd.DataFrame()
    else:
        return pd.DataFrame()

    try:
        return data.astype(float)
    except ValueError:
        return data



def get_ensemble_responses(
    #res: LibresFacade = DEFAULT_LIBRESFACADE,
    db: StorageReader,
    ensemble_id,
):
    res= LibresFacade.from_config_file("/home/frode/poly_ex2/poly/poly.ert")

    t= time.perf_counter()
    ens = db.get_ensemble(ensemble_id)

    print(f"Summary keyset {len(ens.get_summary_keyset())}")
    print(f"Observations: {len(res.get_observations())}")

    ll= []
    name_dict={}
    for obs in res.get_observations():
        name_dict[obs.observation_key] = obs.observation_type

    for name in ens.get_summary_keyset():
        has_observation=False
        if name in name_dict:
            #if obs.observation_type == EnkfObservationImplementationType.SUMMARY_OBS and obs.observation_key == name:
            has_observation=True
            ll.append(name)
                
    print(f"has obs {len(ll)}")
    print(f"time1 {time.perf_counter() - t}")
    t= time.perf_counter()

    for name in res.get_gen_data_keys():
        has_observation=False
        for obs in res.get_observations():
            if obs.observation_type == EnkfObservationImplementationType.GEN_OBS and obs.observation_key == name:
                has_observation=True
                break


    print(f"time2 {time.perf_counter() - t}")









with open_storage("/home/frode/poly_ex2/poly/storage",  mode="r") as storage:
    ensemble= storage.get_ensemble_by_name("default_1")
    #df= data_for_key(ensemble, "PSUM99")
    #print(df)

    get_ensemble_responses(storage, ensemble_id=ensemble.id)

#with open_storage("/home/frode/code/ert/test-data/snake_oil/storage/snake_oil/ensemble",  mode="r") as storage:
    #ensemble= storage.get_ensemble_by_name("default")
    #df= data_for_key(ensemble, "FOPR")
    #print(df)
    #get_ensemble_responses(storage, ensemble_id=ensemble.id)



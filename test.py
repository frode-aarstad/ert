

from typing import Optional, Dict, List
from ert.config.enkf_observation_implementation_type import EnkfObservationImplementationType
from ert.config.gen_data_config import GenDataConfig
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
    elif key in ensemble.get_gen_kw_keyset():
        data = ensemble.load_all_gen_kw_data(key.split(":")[0], realization_index)
        if data.empty:
            return pd.DataFrame()
        data = data[key].to_frame().dropna()
        data.columns = pd.Index([0])
    elif key in ensemble.get_gen_data_keyset():
        key_parts = key.split("@")
        key = key_parts[0]
        report_step = int(key_parts[1]) if len(key_parts) > 1 else 0
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
    res= LibresFacade.from_config_file("/home/frode/poly_ex3/poly/poly.ert")

    t= time.perf_counter()
    ens = db.get_ensemble(ensemble_id)

    print(f"Summary keyset {len(ens.get_summary_keyset())}")
    print(f"Observations: {len(res.get_observations())}")
    print(f"Gen data keys: {len(res.get_gen_data_keys())}")
    print(f"Gen kw: {len(res.get_gen_kw())}")
    
    ll= []
    name_dict={}
    for obs in res.get_observations():
        name_dict[obs.observation_key] = obs.observation_type

    for name in ens.get_summary_keyset():
        if name in name_dict:
            #if obs.observation_type == EnkfObservationImplementationType.SUMMARY_OBS and obs.observation_key == name:
            ll.append(name)
                
    print(f"has obs {len(ll)}")
    print(f"time1 {time.perf_counter() - t}")
    t= time.perf_counter()

    gd=[]
    for key in res.get_gen_data_keys():
        key_parts = key.split("@")
        data_key = key_parts[0]
        data_report_step = int(key_parts[1]) if len(key_parts) > 1 else 0

        obs_key = None

        enkf_obs = res.config.enkf_obs
        for obs_vector in enkf_obs:
            if EnkfObservationImplementationType.GEN_OBS:
                report_step = min(obs_vector.observations.keys())
                key = obs_vector.data_key

                if key == data_key and report_step == data_report_step:
                    obs_key = obs_vector.observation_key
        if obs_key is not None:
            gd.append(obs_key)
        #else:
        #    return []




    print(f"has obs {len(gd)}")
    print(f"time2 {time.perf_counter() - t}")


#@deprecated("Check the experiment for registered responses")
def get_gen_data_keyset(ensemble: EnsembleReader) -> List[str]:
        import time
        t= time.perf_counter()
        keylist = [
            k
            for k, v in ensemble.experiment.response_info.items()
            if "_ert_kind" in v and v["_ert_kind"] == "GenDataConfig"
        ]
        print(f"get_gen_data_keyset 1 {time.perf_counter()-t}") 
        
        keylist2 = [
            k
            for k, v in ensemble.experiment.response_configuration.items()
            if isinstance(v, GenDataConfig)
        ]


        t= time.perf_counter()
        gen_data_list = []
        for k,v in ensemble.experiment.response_configuration.items():
            if isinstance(v, GenDataConfig):
                if v.report_steps is None:
                    gen_data_list.append(f"{k}@0")
                else:
                    for report_step in v.report_steps:
                        gen_data_list.append(f"{k}@{report_step}")
        print(f"get_gen_data_keyset 2 {time.perf_counter()-t}") 
        t= time.perf_counter()
        ss=  sorted(gen_data_list, key=lambda k: k.lower())
        print(f"get_gen_data_keyset 3 {time.perf_counter()-t}") 
        return ss





from ert.shared.storage.extraction import create_priors
def get_experiments(
    db: StorageReader,
) :
    t= time.perf_counter()
    res= LibresFacade.from_config_file("/home/frode/poly_ex2/poly/poly.ert")
    print(f"time1 {time.perf_counter() - t}")







with open_storage("/home/frode/poly_ex3/poly/storage",  mode="r") as storage:
    ensemble= storage.get_ensemble_by_name("default_1")
    df= data_for_key(ensemble, "PSUM99")
    #df= data_for_key(ensemble, "POLY_RES_666@0")
    #df= data_for_key(ensemble, "COEFFS_9:COEFF_0")
    #print(df)

    #get_ensemble_responses(storage, ensemble_id=ensemble.id)
    #get_gen_data_keyset(ensemble)
    #get_experiments(storage)

#with open_storage("/home/frode/code/ert/test-data/snake_oil/storage/snake_oil/ensemble",  mode="r") as storage:
    #ensemble= storage.get_ensemble_by_name("default")
    #df= data_for_key(ensemble, "FOPR")
    #print(df)
    #get_ensemble_responses(storage, ensemble_id=ensemble.id)


